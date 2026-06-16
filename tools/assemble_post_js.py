#!/usr/bin/env python3
"""Assemble per-post Chart.js data files for the blog from generated fragments.

For each post .md in the blog _posts dir, read the canvas ids it references (in
order), look up each id's data-js block from the generated chart_js fragments,
and write the concatenation to <blog>/source/assets/hashtable-bench/<post>.js.

Fragment sources (with precedence — earlier wins on id collision):
  results/m1-max-202305/chart_js        (M1-Max, all timing/lookup/load-factor)
  results/E2388G-202305-native/chart_js (Xeon-E-2388G, + latency)
  results/m1-mem/chart_js               (M1-Max heap_memory_size only)
  results/Xeon-E-2286G-mem/chart_js     (Xeon-E-2286G heap_memory_size only)

Only fragment files whose basename starts with a fresh hyphenated platform name
(M1-Max_/Xeon-E-2388G_/Xeon-E-2286G_) are indexed, so stale underscore-named
fragments and temp_agg.txt are ignored. The canvas id is read from each
fragment's content, not its filename, so it is unambiguous.

The blog checkout lives in a separate repo; pass its path with --blog-root or
the BLOG_ROOT environment variable (no path is hard-coded).
"""
import argparse
import os
import re

BENCH_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# in precedence order (earlier wins)
FRAGMENT_DIRS = [
    os.path.join(BENCH_ROOT, 'results', 'm1-max-202305', 'chart_js'),
    os.path.join(BENCH_ROOT, 'results', 'E2388G-202305-native', 'chart_js'),
    os.path.join(BENCH_ROOT, 'results', 'm1-mem', 'chart_js'),
    os.path.join(BENCH_ROOT, 'results', 'Xeon-E-2286G-mem', 'chart_js'),
]
FRESH_PREFIX = re.compile(r'^(M1-Max|Xeon-E-2388G|Xeon-E-2286G)_')

CANVAS_ID_RE = re.compile(r'canvas id="([^"]+)_chart"')
START_RE = '/* Start of data js for '
END_RE = re.compile(r'/\* End of data js for [^*]+\*/')

# the 12 chart posts (index post handled separately if it has canvases)
POSTS = [
    'int-lookup-throughput', 'int-lookup-latency', 'int-insert-construct',
    'int-erase-insert', 'int-iterate', 'memory-usage-and-load-factor',
    '12-byte-string-lookup', '24-byte-string-lookup', '64-byte-string-lookup',
    'string-insert-construct', 'string-erase-insert', 'string-iterate',
    'hashtable-bench',
]


def extract_block(content):
    """Return (canvas_id, datajs_block) from a fragment file's content."""
    m = CANVAS_ID_RE.search(content)
    if not m:
        return None, None
    cid = m.group(1)
    start = content.find(START_RE)
    if start == -1:
        return cid, None
    endm = END_RE.search(content)
    if endm is None:
        return cid, None
    return cid, content[start:endm.end()]


def build_index():
    index = {}  # canvas_id -> datajs_block
    for d in FRAGMENT_DIRS:
        if not os.path.isdir(d):
            print('  (missing dir) %s' % d)
            continue
        for fn in sorted(os.listdir(d)):
            if not fn.endswith('.txt') or not FRESH_PREFIX.match(fn):
                continue
            with open(os.path.join(d, fn)) as f:
                content = f.read()
            cid, block = extract_block(content)
            if cid is None or block is None:
                continue
            if cid not in index:  # precedence: earlier dir wins
                index[cid] = block
    print('indexed %d unique chart ids from %d dirs' % (len(index), len(FRAGMENT_DIRS)))
    return index


def post_canvas_ids(md_path):
    """Ordered, de-duplicated canvas ids referenced in a post .md."""
    with open(md_path) as f:
        text = f.read()
    ids = []
    seen = set()
    for m in CANVAS_ID_RE.finditer(text):
        cid = m.group(1)
        if cid not in seen:
            seen.add(cid)
            ids.append(cid)
    return ids


def resolve_blog_root(arg_value):
    blog_root = arg_value or os.environ.get('BLOG_ROOT')
    if not blog_root:
        raise SystemExit(
            'error: blog checkout path is required; pass --blog-root '
            '<path-to-blog-repo> or set the BLOG_ROOT environment variable.')
    if not os.path.isdir(blog_root):
        raise SystemExit('error: blog root does not exist: %s' % blog_root)
    return blog_root


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--blog-root', default=None,
                    help='path to the blog repo checkout (or set $BLOG_ROOT)')
    args = ap.parse_args()
    blog_root = resolve_blog_root(args.blog_root)
    posts_dir = os.path.join(blog_root, 'source', '_posts')
    assets_dir = os.path.join(blog_root, 'source', 'assets', 'hashtable-bench')

    index = build_index()
    total_missing = 0
    for post in POSTS:
        md = os.path.join(posts_dir, post + '.md')
        if not os.path.exists(md):
            print('[skip] %s: no .md' % post)
            continue
        ids = post_canvas_ids(md)
        if not ids:
            print('[skip] %s: no canvases' % post)
            continue
        missing = [c for c in ids if c not in index]
        if missing:
            total_missing += len(missing)
            print('[MISSING] %s: %d/%d ids missing:' % (post, len(missing), len(ids)))
            for c in missing[:12]:
                print('    %s' % c)
            if len(missing) > 12:
                print('    ... +%d more' % (len(missing) - 12))
            continue  # do not write a broken file
        out_path = os.path.join(assets_dir, post + '.js')
        with open(out_path, 'w') as f:
            for c in ids:
                f.write(index[c])
                f.write('\n\n\n')
        print('[ok] %s: wrote %d charts -> %s' % (post, len(ids), os.path.basename(out_path)))
    if total_missing:
        print('\nTOTAL MISSING ids: %d (those posts were NOT written)' % total_missing)
    else:
        print('\nAll posts assembled with 0 missing ids.')


if __name__ == '__main__':
    main()
