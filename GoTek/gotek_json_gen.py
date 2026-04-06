#!/usr/bin/env python3
r"""
GoTek JSON Generator for ESP32-S3 Virtual CD-ROM / GoTek firmware.

Scans a directory for image files, matches them against a catalog
spreadsheet (XLSX), and generates order.json and labels.json.

The catalog spreadsheet must contain two columns:
  - Floppy ID   : integer (0-based), matches the numeric part of the filename
  - Description : human-readable label shown in the GoTek web UI

Usage:
    python3 gotek_json_gen.py [gotek_dir] [xlsx_file] [options]

    When run with no arguments, the current directory is scanned for
    image files and the first *.xlsx file found there is used as catalog.

Filename pattern:
    Default pattern matches FIMG000.img, FIMG001.img, ...
    Use --pattern to match a different naming convention, e.g.:
      --pattern "DSKA{id:04d}.img"   → DSKA0000.img, DSKA0001.img
      --pattern "disk{id}.img"       → disk0.img, disk1.img
      --pattern "floppy_{id:03d}.ima"→ floppy_000.ima, floppy_001.ima
    The {id} placeholder marks where the numeric ID appears.
    Width/format spec ({id:04d}) is used only for output filename generation;
    the scanner accepts any number of digits.

Options:
    --pattern TMPL   Filename template with {id} placeholder (default: FIMG{id:03d}.img)
    --sheet N        Sheet index or name to read (default: 0)
    --id-col NAME    Column name for Floppy ID   (auto-detected if omitted)
    --desc-col NAME  Column name for Description  (auto-detected if omitted)
    --dry-run        Preview output without writing files
"""

import sys, os, re, json, argparse, glob
import pandas as pd

# Column name hints for auto-detection (case-insensitive substring match)
_ID_HINTS   = ['floppy id', 'id', 'number', 'num', 'slot']
_DESC_HINTS = ['description', 'desc', 'label', 'name', 'title', 'popis']


# ---------------------------------------------------------------------------
# Filename pattern handling
# ---------------------------------------------------------------------------

_DEFAULT_PATTERN = 'FIMG{id:03d}.img'


def pattern_to_regex(pattern):
    r"""
    Convert a filename template like 'FIMG{id:03d}.img' into a compiled
    regex that captures the numeric ID group.

    The {id} or {id:...} placeholder is replaced with (\d+).
    All other regex special characters in the template are escaped.
    """
    # Strip the format spec from {id:...} → {id}
    clean = re.sub(r'\{id:[^}]*\}', '{id}', pattern)
    # Escape everything except our placeholder
    parts = clean.split('{id}')
    if len(parts) != 2:
        sys.exit(
            f"ERROR: Pattern must contain exactly one {{id}} placeholder.\n"
            f"  Got: {pattern!r}"
        )
    regex_str = re.escape(parts[0]) + r'(\d+)' + re.escape(parts[1])
    return re.compile('^' + regex_str + '$', re.IGNORECASE)


def make_filename(pattern, fid):
    """Render a filename from pattern and integer ID."""
    return pattern.replace('{id}', str(fid)) \
           if '{id}' in pattern else \
           pattern.format(id=fid)


# ---------------------------------------------------------------------------
# XLSX catalog
# ---------------------------------------------------------------------------

def find_column(columns, hints, label):
    cols_lower = [c.lower() for c in columns]
    for hint in hints:
        for i, col in enumerate(cols_lower):
            if hint in col:
                return columns[i]
    available = ', '.join(f'"{c}"' for c in columns)
    sys.exit(
        f"ERROR: Cannot find {label} column.\n"
        f"  Available: {available}\n"
        f"  Use --id-col / --desc-col to specify explicitly."
    )


def find_xlsx(directory):
    hits = glob.glob(os.path.join(directory, '*.xlsx'))
    return hits[0] if hits else None


def load_catalog(xlsx_path, sheet, id_col, desc_col):
    try:
        df = pd.read_excel(xlsx_path, sheet_name=sheet)
    except Exception as e:
        sys.exit(f"ERROR: Cannot read spreadsheet: {e}")
    df.columns = [str(c).strip() for c in df.columns]
    id_col   = id_col   or find_column(list(df.columns), _ID_HINTS,   'Floppy ID')
    desc_col = desc_col or find_column(list(df.columns), _DESC_HINTS, 'Description')
    catalog = {}
    for _, row in df.iterrows():
        fid, desc = row[id_col], row[desc_col]
        if pd.notna(fid):
            try:
                catalog[int(fid)] = str(desc).strip() if pd.notna(desc) else ''
            except (ValueError, TypeError):
                pass
    return catalog, id_col, desc_col


# ---------------------------------------------------------------------------
# Directory scan
# ---------------------------------------------------------------------------

def scan_images(gotek_dir, img_regex):
    found = []
    for fname in os.listdir(gotek_dir):
        m = img_regex.match(fname)
        if m:
            found.append((int(m.group(1)), fname))
    return sorted(found)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description='Generate GoTek order.json and labels.json from an XLSX catalog.',
        epilog=(
            'When run with no arguments, the current directory is scanned for\n'
            'image files and the first *.xlsx file found there is used.'
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument('gotek_dir', nargs='?', default=None,
                    help='Directory with image files (default: current directory)')
    ap.add_argument('xlsx_file', nargs='?', default=None,
                    help='Catalog spreadsheet (default: first *.xlsx in gotek_dir)')
    ap.add_argument('--pattern',  default=_DEFAULT_PATTERN, metavar='TMPL',
                    help=f'Filename template with {{id}} placeholder '
                         f'(default: {_DEFAULT_PATTERN!r})\n'
                         f'Examples: "DSKA{{id:04d}}.img"  "disk{{id}}.ima"')
    ap.add_argument('--sheet',    default=0,    metavar='N',
                    help='Sheet index or name (default: 0)')
    ap.add_argument('--id-col',   default=None, metavar='NAME',
                    help='Column name for Floppy ID (auto-detected)')
    ap.add_argument('--desc-col', default=None, metavar='NAME',
                    help='Column name for Description (auto-detected)')
    ap.add_argument('--dry-run',  action='store_true',
                    help='Preview without writing files')
    args = ap.parse_args()

    # Resolve paths
    gotek_dir = os.path.abspath(args.gotek_dir or os.getcwd())
    if not os.path.isdir(gotek_dir):
        sys.exit(f"ERROR: Directory not found: {gotek_dir}")

    if args.xlsx_file:
        xlsx_path = os.path.abspath(args.xlsx_file)
    else:
        xlsx_path = find_xlsx(gotek_dir)
        if not xlsx_path:
            sys.exit(
                f"ERROR: No *.xlsx file found in: {gotek_dir}\n"
                f"  Place the catalog spreadsheet in the same directory as the\n"
                f"  image files, or pass it as the second argument."
            )
    if not os.path.isfile(xlsx_path):
        sys.exit(f"ERROR: Spreadsheet not found: {xlsx_path}")

    # Compile filename pattern
    img_regex = pattern_to_regex(args.pattern)

    print(f"Directory : {gotek_dir}")
    print(f"Pattern   : {args.pattern!r}  (regex: {img_regex.pattern})")

    # Load catalog
    catalog, id_col, desc_col = load_catalog(
        xlsx_path, args.sheet, args.id_col, args.desc_col
    )
    print(f"Catalog   : {os.path.basename(xlsx_path)}  "
          f"(sheet={args.sheet!r}, id={id_col!r}, desc={desc_col!r})")
    print(f"            {len(catalog)} entries loaded\n")

    # Scan images
    images = scan_images(gotek_dir, img_regex)
    if not images:
        sys.exit(
            f"ERROR: No files matching {args.pattern!r} found in:\n"
            f"  {gotek_dir}\n"
            f"  Use --pattern to specify a different naming convention."
        )

    # Build output
    order, labels, missing = [], {}, []
    for fid, fname in images:
        order.append(fname)
        if fid in catalog:
            labels[fname] = catalog[fid]
        else:
            labels[fname] = ''
            missing.append(fid)

    # Print table
    fw = max(len(f) for _, f in images)
    print(f"{'ID':>6}  {'Filename':<{fw}}  Description")
    print('-' * min(fw + 60, 100))
    for fid, fname in images:
        flag = '  *** no catalog entry ***' if fid in missing else ''
        print(f"{fid:>6}  {fname:<{fw}}  {labels[fname]}{flag}")

    print(f"\nImages  : {len(images)}   "
          f"Matched : {len(images)-len(missing)}   "
          + (f"Unmatched : {len(missing)} (IDs: {missing})" if missing else "All matched"))

    if args.dry_run:
        print("\n=== DRY RUN — no files written ===")
        print("\norder.json:")
        print(json.dumps(order, ensure_ascii=False, indent=2))
        print("\nlabels.json (first 5):")
        print(json.dumps(dict(list(labels.items())[:5]), ensure_ascii=False, indent=2))
        return

    order_path  = os.path.join(gotek_dir, 'order.json')
    labels_path = os.path.join(gotek_dir, 'labels.json')

    with open(order_path,  'w', encoding='utf-8') as f:
        json.dump(order,  f, ensure_ascii=False, separators=(',', ':'))
    with open(labels_path, 'w', encoding='utf-8') as f:
        json.dump(labels, f, ensure_ascii=False, separators=(',', ':'))

    print(f"\nWritten : {order_path}")
    print(f"Written : {labels_path}")
    print("\nCopy both JSON files to the /GoTek directory on the SD card alongside")
    print("the image files. The firmware reads them on next boot or Refresh.")


if __name__ == '__main__':
    main()
