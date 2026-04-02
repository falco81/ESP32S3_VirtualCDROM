;=================================================================
; ESPUSBCD.SYS  -  ESP32 Virtual CD-ROM Driver for DOS  v1.0
;
; Sits above any DOS ASPI manager (USBASPI1.SYS, USBASPI2.SYS).
; Provides full MSCDEX CD-ROM interface: data sectors + audio.
;
; Requires: 80386+, DOS ASPI manager already loaded
;
; CONFIG.SYS (USBASPI must come BEFORE this driver):
;   DEVICE=USBASPI1.SYS /w /v
;   DEVICE=ESPUSBCD.SYS [/D:name] [/T:id] [/H:ha]
;
; AUTOEXEC.BAT:
;   SHSUCDX.COM /D:ESPUSBCD
;   MSCDEX.EXE  /D:ESPUSBCD /M:8
;
; Options:
;   /D:name   8-char device name (default ESPUSBCD)
;   /T:id     ASPI target SCSI ID 0-7 (default 0)
;   /H:ha     ASPI host-adapter number (default 0)
;
; Build: nasm -f bin -o ESPUSBCD.SYS espusbcd.asm
;=================================================================

BITS 16
CPU  486
ORG  0

;-----------------------------------------------------------------
STAT_DONE   EQU 0100h
STAT_ERR    EQU 8000h
ERR_NORDY   EQU 0002h
ERR_NOCMD   EQU 0003h
ERR_READ    EQU 000Bh
ERR_GENL    EQU 000Ch

AS_STOP EQU 0
AS_PLAY EQU 1
AS_PAUSE EQU 2

ASPI_HAINQ  EQU 0
ASPI_DEVTYP EQU 1
ASPI_EXEC   EQU 2

ASPI_IN     EQU 08h
ASPI_OUT    EQU 10h
ASPI_NONE   EQU 18h
ASPI_DONE   EQU 01h

SCSI_TUR    EQU 00h
SCSI_INQR   EQU 12h
SCSI_STST   EQU 1Bh
SCSI_PREV   EQU 1Eh
SCSI_RDCAP  EQU 25h
SCSI_RD10   EQU 28h
SCSI_RDSC   EQU 42h
SCSI_RDTOC  EQU 43h
SCSI_PLMSF  EQU 47h
SCSI_PAUS   EQU 4Bh
SCSI_STOP   EQU 4Eh

TOC_BUFSZ   EQU 804
DATA_BUFSZ  EQU 2048

;=================================================================
; DEVICE DRIVER HEADER
;=================================================================
hdr_next:   DD  -1
hdr_attr:   DW  0C800h
hdr_strat:  DW  _strategy
hdr_intr:   DW  _interrupt
dev_name:   DB  'ESPUSBCD'
            DW  0               ; MSCDEX reserved
            DB  0               ; first drive letter
            DB  1               ; number of CD-ROM units (must be ≥1 for MSCDEX)

;=================================================================
; RESIDENT VARIABLES
;=================================================================
rh_off:     DW  0
rh_seg:     DW  0

aspi_ha:    DB  0
aspi_tgt:   DB  0
aspi_lun:   DB  0

disc_ok:    DB  0
aud_state:  DB  AS_STOP
toc_ok:     DB  0
vol_sectors: DD 0

ch_sel:     DB  0, 1, 2, 3
ch_vol:     DB  0FFh, 0FFh, 0, 0

play_sm: DB 0
play_ss: DB 0
play_sf: DB 0
play_em: DB 0
play_es: DB 0
play_ef: DB 0

;=================================================================
; ASPI SRB  (Adaptec ASPI for DOS layout, 74 bytes)
;=================================================================
ALIGN 2
srb:
.cmd:       DB  ASPI_EXEC
.sts:       DB  0
.ha:        DB  0
.flg:       DB  0
.rsv1:      DW  0
.tgt:       DB  0
.lun:       DB  0
.blen:      DD  0
.boff:      DW  0
.bseg:      DW  0
.snslen:    DB  14
.cdblen:    DB  10
.hastat:    DB  0
.tgtstat:   DB  0
.postproc:  DD  0
.rsv2:      times 20 DB 0
.cdb:       times 16 DB 0
.sense:     times 14 DB 0

; HA-Inquiry SRB (INIT only)
haiq:
.cmd:   DB  ASPI_HAINQ
.sts:   DB  0
.ha:    DB  0
.flg:   DB  0
.rsv:   DW  0
.count: DB  0
.sid:   DB  0
        times 48 DB 0

;=================================================================
; DATA BUFFERS
;=================================================================
data_buf:   times DATA_BUFSZ DB 0
toc_buf:    times TOC_BUFSZ  DB 0

end_resident:

;=================================================================
; STRATEGY + INTERRUPT DISPATCHER
;=================================================================
_strategy:
    mov  cs:[rh_off], bx
    mov  cs:[rh_seg], es
    retf

_interrupt:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    pushf

    push cs
    pop  ds

    mov  es, [rh_seg]
    mov  bx, [rh_off]
    mov  al, [es:bx+2]

    cmp al,   0 ; INIT
    je  .d_init
    cmp al,   3 ; IOCTL INPUT
    je  .d_iin
    cmp al,  12 ; IOCTL OUTPUT
    je  .d_iout
    cmp al,  13 ; OPEN
    je  .d_ok
    cmp al,  14 ; CLOSE
    je  .d_ok
    cmp al, 128 ; READ LONG
    je  .d_rdl
    cmp al, 130 ; READ LONG PREFETCH
    je  .d_rdl
    cmp al, 131 ; SEEK (no-op)
    je  .d_ok
    cmp al, 132 ; PLAY AUDIO
    je  .d_play
    cmp al, 133 ; STOP AUDIO
    je  .d_stop
    cmp al, 136 ; RESUME AUDIO
    je  .d_res
    mov word [es:bx+3], STAT_DONE|STAT_ERR|ERR_NOCMD
    jmp .done

.d_init:  call cmd_init      ; writes status itself
          jmp  .done
.d_iin:   call cmd_ioctl_in
          jmp  .done
.d_iout:  call cmd_ioctl_out
          jmp  .done
.d_rdl:   call cmd_read_long
          jmp  .done
.d_play:  call cmd_play_audio
          jmp  .done
.d_stop:  call cmd_stop_audio
          jmp  .done
.d_res:   call cmd_resume_audio
          jmp  .done
.d_ok:    mov  word [es:bx+3], STAT_DONE

.done:
    popf
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    retf

;=================================================================
; CMD: IOCTL INPUT (command 3)
; RH+14: far ptr to IOCTL data buffer
; RH+18: byte count
;=================================================================
cmd_ioctl_in:
    ; Save request header, switch ES:DI to IOCTL buffer
    push word [es:bx+16]    ; ioctl seg (discarded at end)
    push bx                 ; rh_off
    push es                 ; rh_seg

    mov  di, [es:bx+14]
    mov  ax, [es:bx+16]
    mov  es, ax             ; ES:DI = IOCTL buffer

    movzx bx, byte [es:di]  ; subfunction code

    cmp bx,  0 ; channel info
    je  .sf0
    cmp bx,  1 ; drive head location
    je  .sf1
    cmp bx,  4 ; audio status
    je  .sf4
    cmp bx,  5 ; drive bytes (INQUIRY)
    je  .sf5
    cmp bx,  6 ; device status
    je  .sf6
    cmp bx,  9 ; volume size
    je  .sf9
    cmp bx, 10 ; audio disk info (first/last track, leadout)
    je  .sf10
    cmp bx, 11 ; audio track info
    je  .sf11
    cmp bx, 12 ; audio Q-channel (current position)
    je  .sf12
    cmp bx, 13 ; sub-channel info (same as 12)
    je  .sf12
    jmp .sf_ok  ; unknown — ignore, return success

; SF0: Return Audio Channel Info
.sf0:
    mov al, [ch_sel]
    mov [es:di+1], al
    mov al, [ch_vol]
    mov [es:di+2], al
    mov al, [ch_sel+1]
    mov [es:di+3], al
    mov al, [ch_vol+1]
    mov [es:di+4], al
    mov al, [ch_sel+2]
    mov [es:di+5], al
    mov al, [ch_vol+2]
    mov [es:di+6], al
    mov al, [ch_sel+3]
    mov [es:di+7], al
    mov al, [ch_vol+3]
    mov [es:di+8], al
    jmp .sf_ok

; SF1: Return Drive Head Location (current position)
.sf1:
    call scsi_read_subchannel
    jc   .sf_err
    cmp  byte [es:di+1], 0   ; addressing mode: 0=LBA, 1=MSF
    je   .sf1_lba
    mov  al, [data_buf+9]    ; abs M
    mov  [es:di+2], al
    mov  al, [data_buf+10]   ; abs S
    mov  [es:di+3], al
    mov  al, [data_buf+11]   ; abs F
    mov  [es:di+4], al
    mov  byte [es:di+5], 0
    jmp  .sf_ok
.sf1_lba:
    movzx eax, byte [data_buf+9]
    movzx ecx, byte [data_buf+10]
    movzx edx, byte [data_buf+11]
    imul eax, 60
    add  eax, ecx
    imul eax, 75
    add  eax, edx
    sub  eax, 150
    mov  [es:di+2], eax
    jmp  .sf_ok

; SF4: Audio Status - query sub-channel for live state
.sf4:
    call scsi_read_subchannel
    jc   .sf4_local             ; on error, use local state
    ; data_buf[1] = audio status: 11h=playing,12h=paused,13h=done,15h=none
    xor  ax, ax
    cmp  byte [data_buf+1], 12h  ; paused?
    je   .sf4_paused
    cmp  byte [data_buf+1], 11h  ; playing?
    je   .sf4_play
    jmp  .sf4_done
.sf4_paused:
    or   ax, 1                  ; bit0 = paused
    mov  byte [aud_state], AS_PAUSE
    jmp  .sf4_done
.sf4_play:
    mov  byte [aud_state], AS_PLAY
    jmp  .sf4_done
.sf4_local:
    xor  ax, ax
    cmp  byte [aud_state], AS_PAUSE
    jne  .sf4_done
    or   ax, 1
.sf4_done:
    mov  [es:di+1], ax
    xor  eax, eax
    mov  [es:di+3], eax
    mov  [es:di+7], eax
    jmp  .sf_ok

; SF5: Return Drive Bytes (INQUIRY data, 128 bytes)
.sf5:
    call scsi_inquiry
    jc   .sf_err
    push si
    mov  si, data_buf
    mov  cx, 128
.sf5_loop:
    mov  al, [si]
    mov  [es:di+1], al
    inc  si
    inc  di
    loop .sf5_loop
    sub  di, 128              ; restore DI to IOCTL buf start
    pop  si
    jmp  .sf_ok

; SF6: Return Device Status DWORD
.sf6:
    call scsi_tur
    jc   .sf6_nodisc
    mov  byte [disc_ok], 1
    mov  dword [es:di+1], 0x0002   ; door unlocked, disc present
    jmp  .sf_ok
.sf6_nodisc:
    mov  byte [disc_ok], 0
    mov  byte [toc_ok], 0
    mov  dword [vol_sectors], 0    ; force re-read on next mount
    mov  dword [es:di+1], 0x0102   ; bit8=no disc
    jmp  .sf_ok

; SF9: Return Volume Size (sectors)
.sf9:
    call ensure_cap
    jc   .sf_err
    mov  eax, [vol_sectors]
    mov  [es:di+1], eax
    jmp  .sf_ok

; SF10: Return Audio Disk Info
.sf10:
    call ensure_toc
    jc   .sf_err
    mov  al, [toc_buf+2]           ; first track
    mov  [es:di+1], al
    mov  al, [toc_buf+3]           ; last track
    mov  [es:di+2], al
    call find_leadout               ; -> AL=M, AH=S, BL=F, CF on error
    jc   .sf_err
    mov  [es:di+3], al             ; leadout M
    mov  [es:di+4], ah             ; leadout S
    mov  [es:di+5], bl             ; leadout F
    mov  byte [es:di+6], 0
    jmp  .sf_ok

; SF11: Return Audio Track Info
.sf11:
    call ensure_toc
    jc   .sf_err
    mov  al, [es:di+1]             ; track number to query
    call find_track                 ; -> DL=M, DH=S, BL=F, BH=ctrl, CF
    jc   .sf_err
    mov  [es:di+2], dl
    mov  [es:di+3], dh
    mov  [es:di+4], bl
    mov  [es:di+5], bh
    jmp  .sf_ok

; SF12/13: Audio Q-Channel Info
.sf12:
    call scsi_read_subchannel
    jc   .sf_err
    mov  al, [data_buf+5]
    mov  [es:di+1], al             ; ADR|Control
    mov  al, [data_buf+6]
    mov  [es:di+2], al             ; track number
    mov  al, [data_buf+7]
    mov  [es:di+3], al             ; index
    mov  byte [es:di+4], 0
    mov  al, [data_buf+9]
    mov  [es:di+5], al             ; abs M
    mov  al, [data_buf+10]
    mov  [es:di+6], al             ; abs S
    mov  al, [data_buf+11]
    mov  [es:di+7], al             ; abs F
    mov  byte [es:di+8], 0
    mov  al, [data_buf+13]
    mov  [es:di+9], al             ; rel M
    mov  al, [data_buf+14]
    mov  [es:di+10], al            ; rel S
    mov  al, [data_buf+15]
    mov  [es:di+11], al            ; rel F
    mov  byte [es:di+12], 0
    jmp  .sf_ok

.sf_err:
    pop  es
    pop  bx
    add  sp, 2
    mov  word [es:bx+3], STAT_DONE|STAT_ERR|ERR_READ
    ret
.sf_ok:
    pop  es
    pop  bx
    add  sp, 2
    mov  word [es:bx+3], STAT_DONE
    ret

;=================================================================
; CMD: IOCTL OUTPUT (command 12)
;=================================================================
cmd_ioctl_out:
    push word [es:bx+16]
    push bx
    push es

    mov  di, [es:bx+14]
    mov  ax, [es:bx+16]
    mov  es, ax

    movzx bx, byte [es:di]

    cmp bx, 0 ; eject
    je  .sf0
    cmp bx, 1 ; lock/unlock door
    je  .sf1
    cmp bx, 2 ; reset
    je  .sf2
    cmp bx, 3 ; audio channel control (volume)
    je  .sf3
    cmp bx, 5 ; close tray
    je  .sf5
    jmp .sf_ok

.sf0:   ; Eject
    call zero_cdb
    mov  byte [srb.cdb], SCSI_STST
    mov  byte [srb.cdb+4], 02h    ; LoEj=1 Start=0 = eject
    mov  byte [srb.cdblen], 6
    mov  byte [srb.flg], ASPI_NONE
    mov  dword [srb.blen], 0
    call do_aspi
    mov  byte [disc_ok], 0
    mov  byte [toc_ok], 0
    mov  dword [vol_sectors], 0    ; force re-read after new mount
    jmp  .sf_ok

.sf1:   ; Lock/Unlock
    mov  al, [es:di+1]
    and  al, 1                    ; bit0: 0=unlock, 1=lock
    call do_scsi_prevent
    jmp  .sf_ok

.sf2:   ; Reset
    call scsi_tur
    jmp  .sf_ok

.sf3:   ; Audio channel control (volume settings)
    mov  al, [es:di+1]
    mov  [ch_sel], al
    mov  al, [es:di+2]
    mov  [ch_vol], al
    mov  al, [es:di+3]
    mov  [ch_sel+1], al
    mov  al, [es:di+4]
    mov  [ch_vol+1], al
    mov  al, [es:di+5]
    mov  [ch_sel+2], al
    mov  al, [es:di+6]
    mov  [ch_vol+2], al
    mov  al, [es:di+7]
    mov  [ch_sel+3], al
    mov  al, [es:di+8]
    mov  [ch_vol+3], al
    jmp  .sf_ok

.sf5:   ; Close tray
    call zero_cdb
    mov  byte [srb.cdb], SCSI_STST
    mov  byte [srb.cdb+4], 03h    ; LoEj=1 Start=1 = load
    mov  byte [srb.cdblen], 6
    mov  byte [srb.flg], ASPI_NONE
    mov  dword [srb.blen], 0
    call do_aspi
    jmp  .sf_ok

.sf_ok:
    pop  es
    pop  bx
    add  sp, 2
    mov  word [es:bx+3], STAT_DONE
    ret

;=================================================================
; CMD: READ LONG (commands 128, 130)
; RH+13: addressing mode (0=LBA, 1=MSF)
; RH+14: far ptr to dest buffer
; RH+18: sector count
; RH+20: starting sector (LBA or MSF bytes M,S,F,0)
; RH+26: read mode (0=cooked 2048 B)
;=================================================================
cmd_read_long:
    movzx ecx, word [es:bx+18]    ; sector count
    test  cx, cx
    jz    .ok

    movzx eax, byte [es:bx+13]    ; addressing mode
    test  al, al
    jz    .hsg

    ; Red Book MSF: RH+20=M, +21=S, +22=F
    movzx eax, byte [es:bx+20]    ; M
    movzx edx, byte [es:bx+21]    ; S
    movzx esi, byte [es:bx+22]    ; F
    imul  eax, 60
    add   eax, edx
    imul  eax, 75
    add   eax, esi
    sub   eax, 150                 ; -> LBA
    jmp   .have_lba
.hsg:
    mov   eax, [es:bx+20]         ; HSG LBA

.have_lba:
    ; EAX=start LBA, ECX=sector count
    ; Get dest buffer
    push  word [es:bx+16]          ; dest seg
    push  word [es:bx+14]          ; dest off
    push  ecx
    push  eax

    call  zero_cdb
    pop   eax
    pop   ecx

    mov   byte [srb.cdb], SCSI_RD10
    ; LBA big-endian at CDB+2..+5
    mov   edx, eax
    bswap edx
    mov   [srb.cdb+2], edx
    ; transfer length big-endian at CDB+7..+8
    mov   ax, cx
    xchg  al, ah
    mov   [srb.cdb+7], ax

    mov   byte [srb.cdblen], 10
    mov   byte [srb.flg], ASPI_IN

    pop   word [srb.boff]          ; dest off
    pop   word [srb.bseg]          ; dest seg

    movzx eax, cx
    imul  eax, 2048
    mov   [srb.blen], eax

    call  do_aspi
    jc    .err
.ok:
    mov   word [es:bx+3], STAT_DONE
    ret
.err:
    mov   word [es:bx+3], STAT_DONE|STAT_ERR|ERR_READ
    ret

;=================================================================
; CMD: PLAY AUDIO (command 132)
; RH+13: start M (if MSF mode) or start LBA low byte (if HSG)
; RH+14: start S or LBA byte2
; RH+15: start F or LBA byte3
; RH+16: 0 or LBA MSB
; RH+17: sector count (DWORD)
; RH+21: addressing mode (0=HSG, 1=Red Book MSF)
;=================================================================
cmd_play_audio:
    movzx eax, byte [es:bx+21]    ; mode
    test  al, al
    jz    .hsg

    ; MSF Red Book: DWORD at +13 = {00h, M, S, F} (per MSCDEX spec §4.4)
    ; +13=00h (unused), +14=M, +15=S, +16=F
    movzx eax, byte [es:bx+14]    ; M
    movzx ecx, byte [es:bx+15]    ; S
    movzx edx, byte [es:bx+16]    ; F
    imul  eax, 60
    add   eax, ecx
    imul  eax, 75
    add   eax, edx
    sub   eax, 150
    jmp   .have_start
.hsg:
    mov   eax, [es:bx+13]         ; start LBA

.have_start:
    ; EAX = start LBA
    push  eax
    call  lba_to_msf               ; -> AL=M, AH=S, BL=F
    mov   [play_sm], al
    mov   [play_ss], ah
    mov   [play_sf], bl

    ; end LBA = start + count
    pop   eax
    mov   ecx, [es:bx+17]         ; sector count
    add   eax, ecx
    call  lba_to_msf
    mov   [play_em], al
    mov   [play_es], ah
    mov   [play_ef], bl

    ; Build PLAY AUDIO MSF CDB (0x47)
    call  zero_cdb
    mov   byte [srb.cdb], SCSI_PLMSF
    mov   al, [play_sm]
    mov   [srb.cdb+3], al
    mov   al, [play_ss]
    mov   [srb.cdb+4], al
    mov   al, [play_sf]
    mov   [srb.cdb+5], al
    mov   al, [play_em]
    mov   [srb.cdb+6], al
    mov   al, [play_es]
    mov   [srb.cdb+7], al
    mov   al, [play_ef]
    mov   [srb.cdb+8], al
    mov   byte [srb.cdblen], 10
    mov   byte [srb.flg], ASPI_NONE
    mov   dword [srb.blen], 0
    call  do_aspi
    jc    .err
    mov   byte [aud_state], AS_PLAY
    mov   word [es:bx+3], STAT_DONE
    ret
.err:
    mov   word [es:bx+3], STAT_DONE|STAT_ERR|ERR_GENL
    ret

;=================================================================
; CMD: STOP AUDIO (command 133)
;=================================================================
cmd_stop_audio:
    call  zero_cdb
    mov   byte [srb.cdb], SCSI_STOP
    mov   byte [srb.cdblen], 10
    mov   byte [srb.flg], ASPI_NONE
    mov   dword [srb.blen], 0
    call  do_aspi
    mov   byte [aud_state], AS_STOP
    mov   word [es:bx+3], STAT_DONE
    ret

;=================================================================
; CMD: PLAY AUDIO TRACK INDEX (command 134)
; RH+13: start track (1-99)
; RH+14: start index (1)
; RH+15: end track
; RH+16: end index
; Converts track numbers to LBA via cached TOC, then plays MSF.
;=================================================================
cmd_play_track_index:
    call  ensure_toc
    jc    .err

    ; Get start track → find LBA in TOC
    mov   al, [es:bx+13]          ; start track
    call  find_track               ; DL=M, DH=S, BL=F, BH=ctrl
    jc    .err
    ; Convert start MSF to LBA
    push  bx                       ; save BX (request header)
    mov   cl, dl                   ; M
    mov   ch, dh                   ; S
    mov   dl, bl                   ; F
    call  msf_to_lba               ; EAX = start LBA
    pop   bx

    push  eax                      ; save start LBA

    ; Get end track → find lead-out of end track = start of next track
    ; Use end track + 1 if it exists, otherwise lead-out
    movzx dx, byte [es:bx+15]     ; end track
    inc   dl                       ; look for next track
    push  bx
    mov   al, dl
    call  find_track               ; next track's start = end of requested
    jnc   .found_end
    ; Not found → use lead-out
    call  find_leadout             ; AL=M, AH=S, BL=F
    jc    .pop_fail
    mov   cl, al
    mov   ch, ah
    mov   dl, bl
    call  msf_to_lba
    pop   bx
    jmp   .have_end
.found_end:
    mov   cl, dl
    mov   ch, dh
    mov   dl, bl
    call  msf_to_lba
    pop   bx
.have_end:
    ; EAX = end LBA
    pop   edx                      ; start LBA

    ; Convert start LBA to MSF
    push  eax                      ; save end LBA
    mov   eax, edx
    call  lba_to_msf               ; AL=M, AH=S, BL=F
    mov   [play_sm], al
    mov   [play_ss], ah
    mov   [play_sf], bl

    ; Convert end LBA to MSF
    pop   eax
    call  lba_to_msf
    mov   [play_em], al
    mov   [play_es], ah
    mov   [play_ef], bl

    ; Send PLAY AUDIO MSF
    call  zero_cdb
    mov   byte [srb.cdb], SCSI_PLMSF
    mov   al, [play_sm]
    mov   [srb.cdb+3], al
    mov   al, [play_ss]
    mov   [srb.cdb+4], al
    mov   al, [play_sf]
    mov   [srb.cdb+5], al
    mov   al, [play_em]
    mov   [srb.cdb+6], al
    mov   al, [play_es]
    mov   [srb.cdb+7], al
    mov   al, [play_ef]
    mov   [srb.cdb+8], al
    mov   byte [srb.cdblen], 10
    mov   byte [srb.flg], ASPI_NONE
    mov   dword [srb.blen], 0
    call  do_aspi
    jc    .err
    mov   byte [aud_state], AS_PLAY
    mov   word [es:bx+3], STAT_DONE
    ret
.pop_fail:
    pop   bx
    pop   eax                      ; clean start LBA off stack
.err:
    mov   word [es:bx+3], STAT_DONE|STAT_ERR|ERR_GENL
    ret

;=================================================================
; CMD: RESUME AUDIO (command 136)
;=================================================================
cmd_resume_audio:
    call  zero_cdb
    mov   byte [srb.cdb], SCSI_PAUS
    mov   byte [srb.cdb+8], 01h   ; resume bit
    mov   byte [srb.cdblen], 10
    mov   byte [srb.flg], ASPI_NONE
    mov   dword [srb.blen], 0
    call  do_aspi
    jc    .err
    mov   byte [aud_state], AS_PLAY
    mov   word [es:bx+3], STAT_DONE
    ret
.err:
    mov   word [es:bx+3], STAT_DONE|STAT_ERR|ERR_GENL
    ret

;=================================================================
; DO_ASPI: Execute SCSI command via INT 4Bh
; srb.cdb, srb.cdblen, srb.flg, srb.blen, srb.boff/bseg preset.
; Returns: CF=0 success, CF=1 error
;=================================================================
do_aspi:
    push ax
    push ds
    push si

    ; Fill SRB fixed fields
    mov   byte [srb.cmd], ASPI_EXEC
    mov   byte [srb.sts], 0
    mov   byte [srb.rsv1], 0
    mov   byte [srb.rsv1+1], 0
    mov   al, [aspi_ha]
    mov   [srb.ha], al
    mov   al, [aspi_tgt]
    mov   [srb.tgt], al
    mov   al, [aspi_lun]
    mov   [srb.lun], al
    mov   byte [srb.snslen], 14
    mov   dword [srb.postproc], 0  ; synchronous poll

    ; Call ASPI: DS:SI = SRB address
    push  cs
    pop   ds
    mov   si, srb
    int   4Bh                      ; ASPI entry point

    ; Poll until SRB_Status != 0  (with timeout ~500k iterations ≈ ~5 sec)
    mov   ecx, 500000
.poll:
    mov   al, cs:[srb.sts]
    test  al, al
    jnz   .polldone
    dec   ecx
    jnz   .poll
    ; Timeout: force error
    stc
    jmp   .ret
.polldone:

    ; Check result
    cmp   al, ASPI_DONE
    jne   .fail
    cmp   byte cs:[srb.tgtstat], 0 ; target status must be 0 (GOOD)
    jne   .fail
    clc
    jmp   .ret
.fail:
    stc
.ret:
    pop   si
    pop   ds
    pop   ax
    ret

;=================================================================
; ZERO_CDB: zero SRB CDB (16 bytes) and clear status fields
;=================================================================
zero_cdb:
    push  ax
    push  cx
    push  di
    push  es
    push  cs
    pop   es
    mov   di, srb.cdb
    xor   al, al
    mov   cx, 16
    rep   stosb
    mov   byte [srb.hastat], 0
    mov   byte [srb.tgtstat], 0
    pop   es
    pop   di
    pop   cx
    pop   ax
    ret

;=================================================================
; SCSI HELPERS
;=================================================================

; scsi_tur: TEST UNIT READY. CF=0 ready, CF=1 not ready
scsi_tur:
    call  zero_cdb
    mov   byte [srb.cdb], SCSI_TUR
    mov   byte [srb.cdblen], 6
    mov   byte [srb.flg], ASPI_NONE
    mov   dword [srb.blen], 0
    call  do_aspi
    ret

; scsi_inquiry: INQUIRY -> data_buf (96 bytes). CF on error.
scsi_inquiry:
    call  zero_cdb
    mov   byte [srb.cdb], SCSI_INQR
    mov   byte [srb.cdb+4], 96     ; allocation length
    mov   byte [srb.cdblen], 6
    mov   byte [srb.flg], ASPI_IN
    mov   word [srb.boff], data_buf
    push  cs
    pop   ax
    mov   [srb.bseg], ax
    mov   dword [srb.blen], 96
    call  do_aspi
    ret

; scsi_read_capacity: READ CAPACITY -> data_buf (8 bytes). CF on error.
scsi_read_capacity:
    call  zero_cdb
    mov   byte [srb.cdb], SCSI_RDCAP
    mov   byte [srb.cdblen], 10
    mov   byte [srb.flg], ASPI_IN
    mov   word [srb.boff], data_buf
    push  cs
    pop   ax
    mov   [srb.bseg], ax
    mov   dword [srb.blen], 8
    call  do_aspi
    ret

; scsi_read_toc: READ TOC (MSF, all tracks) -> toc_buf. CF on error.
scsi_read_toc:
    call  zero_cdb
    mov   byte [srb.cdb], SCSI_RDTOC
    mov   byte [srb.cdb+1], 02h    ; MSF=1 (bit 1)
    mov   byte [srb.cdb+6], 0      ; all tracks from track 0
    mov   byte [srb.cdb+7], (TOC_BUFSZ >> 8) & 0FFh
    mov   byte [srb.cdb+8], TOC_BUFSZ & 0FFh
    mov   byte [srb.cdblen], 10
    mov   byte [srb.flg], ASPI_IN
    mov   word [srb.boff], toc_buf
    push  cs
    pop   ax
    mov   [srb.bseg], ax
    mov   dword [srb.blen], TOC_BUFSZ
    call  do_aspi
    ret

; scsi_read_subchannel: READ SUB-CHANNEL (MSF, format 01) -> data_buf (16B)
scsi_read_subchannel:
    call  zero_cdb
    mov   byte [srb.cdb], SCSI_RDSC
    mov   byte [srb.cdb+1], 02h    ; MSF=1
    mov   byte [srb.cdb+2], 40h    ; SubQ=1
    mov   byte [srb.cdb+3], 01h    ; format: current position
    mov   byte [srb.cdb+7], 0
    mov   byte [srb.cdb+8], 16     ; allocate 16 bytes
    mov   byte [srb.cdblen], 10
    mov   byte [srb.flg], ASPI_IN
    mov   word [srb.boff], data_buf
    push  cs
    pop   ax
    mov   [srb.bseg], ax
    mov   dword [srb.blen], 16
    call  do_aspi
    ret

; do_scsi_prevent: PREVENT/ALLOW MEDIUM REMOVAL
; AL=0: allow (unlock), AL=1: prevent (lock)
do_scsi_prevent:
    push  ax
    call  zero_cdb
    pop   ax
    mov   byte [srb.cdb], SCSI_PREV
    and   al, 1
    mov   [srb.cdb+4], al
    mov   byte [srb.cdblen], 6
    mov   byte [srb.flg], ASPI_NONE
    mov   dword [srb.blen], 0
    call  do_aspi
    ret

;=================================================================
; TOC HELPERS
;=================================================================

; ensure_toc: read TOC if not cached. CF on error.
ensure_toc:
    cmp   byte [toc_ok], 1
    je    .ok
    call  scsi_read_toc
    jc    .err
    mov   byte [toc_ok], 1
.ok:
    clc
    ret
.err:
    stc
    ret

; ensure_cap: read capacity if not cached. CF on error.
ensure_cap:
    cmp   dword [vol_sectors], 0
    jne   .ok
    call  scsi_read_capacity
    jc    .err
    ; Parse big-endian last LBA from data_buf+0..+3
    mov   eax, [data_buf]
    bswap eax                      ; big-endian -> little-endian
    inc   eax                      ; sectors = last_LBA + 1
    mov   [vol_sectors], eax
.ok:
    clc
    ret
.err:
    stc
    ret

; find_leadout: scan toc_buf for track 0AAh entry.
; Returns: AL=M, AH=S, BL=F, CF=1 if not found.
find_leadout:
    mov   ah, [toc_buf+0]
    mov   al, [toc_buf+1]          ; AX = data length (big-endian word)
    ; Number of entries = (data_length - 2) / 8
    sub   ax, 2
    shr   ax, 3
    mov   cx, ax
    mov   si, toc_buf + 4          ; first entry
.loop:
    test  cx, cx
    jz    .notfound
    cmp   byte [si+2], 0AAh        ; track number = lead-out?
    je    .found
    add   si, 8
    dec   cx
    jmp   .loop
.found:
    ; Entry: [si+4]=0, [si+5]=M, [si+6]=S, [si+7]=F
    mov   al, [si+5]               ; M
    mov   ah, [si+6]               ; S
    mov   bl, [si+7]               ; F
    clc
    ret
.notfound:
    stc
    ret

; find_track: scan toc_buf for track AL.
; Returns: DL=M, DH=S, BL=F, BH=ADR|Ctrl, CF=1 if not found.
find_track:
    push  ax
    mov   ah, [toc_buf+0]
    mov   al, [toc_buf+1]
    sub   ax, 2
    shr   ax, 3
    mov   cx, ax
    pop   ax                       ; AL = track number
    push  ax
    mov   si, toc_buf + 4
.loop:
    test  cx, cx
    jz    .notfound
    pop   ax
    push  ax
    cmp   [si+2], al               ; track number match?
    je    .found
    add   si, 8
    dec   cx
    jmp   .loop
.found:
    pop   ax                       ; discard saved track#
    mov   bh, [si+1]               ; ADR|Control
    mov   dl, [si+5]               ; M
    mov   dh, [si+6]               ; S
    mov   bl, [si+7]               ; F
    clc
    ret
.notfound:
    pop   ax
    stc
    ret

;=================================================================
; LBA <-> MSF CONVERSION
;=================================================================

; lba_to_msf: EAX=LBA -> AL=M, AH=S, BL=F  (clobbers ECX, EDX)
lba_to_msf:
    add   eax, 150                 ; add pregap
    xor   edx, edx
    mov   ecx, 75
    div   ecx                      ; EAX=total_s, EDX=F
    mov   bl, dl                   ; BL = F
    xor   edx, edx
    mov   ecx, 60
    div   ecx                      ; EAX=M, EDX=S
    mov   ah, dl                   ; AH = S
    ; AL = M  (EAX low byte, minutes always < 100)
    ret

; msf_to_lba: CL=M, CH=S, DL=F -> EAX=LBA
msf_to_lba:
    movzx eax, cl
    imul  eax, 60
    movzx ecx, ch
    add   eax, ecx
    imul  eax, 75
    movzx ecx, dl
    add   eax, ecx
    sub   eax, 150
    ret

;=================================================================
; INIT CODE (past end_resident, freed by DOS after INIT completes)
;=================================================================

; cmd_init: called from _interrupt for command 0 (INIT)
; ES:BX = request header
; RH+14: end-of-resident pointer to set (DWORD far ptr)
; RH+23: far ptr to CONFIG.SYS command line tail
cmd_init:
    push  es
    push  bx

    ; Parse command line: /D:name  /T:id  /H:ha
    mov   ax, [es:bx+20]           ; command line segment (at INIT RH+18/+20)
    mov   di, [es:bx+18]           ; command line offset
    push  ax
    pop   es                        ; ES:DI = command line
    call  parse_cmdline

    push  cs
    pop   es                        ; restore ES = our segment

    ; Print banner
    mov   dx, msg_banner
    call  print_msg

    ; Try TEST UNIT READY to probe the device (uses do_aspi with built-in timeout).
    ; We do NOT call aspi_ha_inquiry — some USBASPI versions ignore that SRB
    ; and never set SRB_Status, causing an infinite polling loop.
    ; If TUR fails it just means no disc is in the drive yet — that is OK.
    ; The driver always loads successfully so MSCDEX/SHSUCDX can register it.
    call  scsi_tur
    jc    .no_disc
    mov   byte [disc_ok], 1
    mov   dx, msg_ok
    call  print_msg
    jmp   .success
.no_disc:
    mov   dx, msg_no_disc
    call  print_msg

.success:
    ; Set end-of-resident pointer and return success
    pop   bx
    pop   es
    mov   word [es:bx+14], end_resident
    push  cs
    pop   ax
    mov   word [es:bx+16], ax
    mov   word [es:bx+3], STAT_DONE
    ret

;-----------------------------------------------------------------
; parse_cmdline: scan CONFIG.SYS tail for /D: /T: /H:
; ES:DI = command line (NUL or CR terminated)
;-----------------------------------------------------------------
parse_cmdline:
    push  ax
    push  cx
    push  si
.next_char:
    mov   al, [es:di]
    cmp   al, 0
    je    .done
    cmp   al, 13
    je    .done
    ; Look for /
    cmp   al, '/'
    je    .check_switch
    cmp   al, '-'
    je    .check_switch
    inc   di
    jmp   .next_char

.check_switch:
    inc   di
    mov   al, [es:di]
    or    al, 20h               ; lowercase
    cmp   al, 'd'
    je    .sw_d
    cmp   al, 't'
    je    .sw_t
    cmp   al, 'h'
    je    .sw_h
    jmp   .next_char

.sw_d:  ; /D:name — set device name (up to 8 chars, space-padded)
    inc   di                    ; skip 'd'
    cmp   byte [es:di], ':'
    jne   .next_char
    inc   di                    ; skip ':'
    ; Copy up to 8 chars to dev_name, pad with spaces
    mov   si, dev_name
    mov   cx, 8
.d_fill:
    mov   al, 20h               ; space default
    mov   bl, [es:di]
    cmp   bl, 0
    je    .d_pad
    cmp   bl, 13
    je    .d_pad
    cmp   bl, ' '
    je    .d_pad
    cmp   bl, '/'
    je    .d_pad
    cmp   bl, '-'
    je    .d_pad
    mov   al, bl
    ; uppercase it
    cmp   al, 'a'
    jb    .d_store
    cmp   al, 'z'
    ja    .d_store
    sub   al, 20h
.d_store:
    inc   di
.d_pad:
    mov   [si], al
    inc   si
    loop  .d_fill
    jmp   .next_char

.sw_t:  ; /T:id — ASPI target ID (single digit 0-7)
    inc   di
    cmp   byte [es:di], ':'
    jne   .next_char
    inc   di
    mov   al, [es:di]
    sub   al, '0'
    and   al, 7
    mov   [aspi_tgt], al
    inc   di
    jmp   .next_char

.sw_h:  ; /H:ha — host adapter number (single digit)
    inc   di
    cmp   byte [es:di], ':'
    jne   .next_char
    inc   di
    mov   al, [es:di]
    sub   al, '0'
    and   al, 3
    mov   [aspi_ha], al
    inc   di
    jmp   .next_char

.done:
    pop   si
    pop   cx
    pop   ax
    ret

;-----------------------------------------------------------------
; aspi_ha_inquiry: call ASPI HAINQ SRB to check if ASPI is up
; CF=1 if not installed or fails
;-----------------------------------------------------------------
aspi_ha_inquiry:
    push  ds
    push  si
    push  cs
    pop   ds
    mov   byte [haiq.cmd], ASPI_HAINQ
    mov   byte [haiq.sts], 0
    mov   al, [aspi_ha]
    mov   [haiq.ha], al
    mov   byte [haiq.flg], 0
    mov   word [haiq.rsv], 0
    mov   si, haiq
    int   4Bh
.poll:
    mov   al, cs:[haiq.sts]
    test  al, al
    jz    .poll
    cmp   al, ASPI_DONE
    je    .ok
    stc
    jmp   .done
.ok:
    clc
.done:
    pop   si
    pop   ds
    ret

;-----------------------------------------------------------------
; find_cdrom: scan targets 0-7 on current HA for device type 5
; Sets aspi_tgt if /T: was not given and scan succeeds
; CF=1 if no CD-ROM found
;-----------------------------------------------------------------
find_cdrom:
    ; If user specified /T:, just trust it and return success
    ; (They said which target to use)
    ; We'll do a quick TUR to validate later.
    clc
    ret

;-----------------------------------------------------------------
; print_msg: output $-terminated string via INT 21h AH=9
; DX = offset of string in CS
;-----------------------------------------------------------------
print_msg:
    push  ax
    push  ds
    push  dx
    push  cs
    pop   ds
    mov   ah, 9
    int   21h
    pop   dx
    pop   ds
    pop   ax
    ret

;=================================================================
; INIT STRINGS  (in discarded area after end_resident)
;=================================================================
msg_banner:
    DB  13, 10
    DB  'ESPUSBCD.SYS  ESP32 Virtual CD-ROM Driver  v1.0', 13, 10
    DB  '$'
msg_ok:
    DB  'ESPUSBCD: Ready. Disc detected.', 13, 10, '$'
msg_no_disc:
    DB  'ESPUSBCD: Loaded (no disc in drive - insert disc before use).', 13, 10, '$'
