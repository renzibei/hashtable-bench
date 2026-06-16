#!/usr/bin/env python3
"""Print the data story of a post's charts for grounded writing.

For each chart function in a post .js, print the default-visible (hidden:false)
series and their values at representative element counts, plus the visible-line
ranking (by mean rank across sizes). This is the factual basis for the analysis
prose — do not assert anything not visible here.

The blog checkout lives in a separate repo; pass its path with --blog-root or
the BLOG_ROOT environment variable (no path is hard-coded).

Usage: chart_story.py <post-name> [substr-filter] [--blog-root <path>]
"""
import argparse
import os
import re

REP_N = [1024, 32768, 200000, 1200000, 10000000]  # representative element counts


def parse_funcs(js):
    funcs = []
    for m in re.finditer(r'function ([A-Za-z0-9_]+)_create\(\)', js):
        nxt = js.find('function ', m.end())
        funcs.append((m.group(1), js[m.start(): nxt if nxt != -1 else len(js)]))
    return funcs


def parse_block(block):
    lm = re.search(r'_labels = (\[[^\]]*\]);', block)
    labels = [int(x) for x in re.findall(r'\d+', lm.group(1))] if lm else []
    datasets = []
    for dm in re.finditer(r"label: '([^']+)',", block):
        seg = block[dm.end(): dm.end() + 600]
        hidden = re.search(r'hidden: (\w+)', seg).group(1)
        data = re.search(r'data: \[([^\]]*)\]', seg).group(1)
        vals = [float(x) for x in data.split(',')] if data.strip() else []
        datasets.append((dm.group(1), hidden == 'false', vals))
    return labels, datasets


def resolve_assets(arg_value):
    blog_root = arg_value or os.environ.get('BLOG_ROOT')
    if not blog_root:
        raise SystemExit(
            'error: blog checkout path is required; pass --blog-root '
            '<path-to-blog-repo> or set the BLOG_ROOT environment variable.')
    assets = os.path.join(blog_root, 'source', 'assets', 'hashtable-bench')
    if not os.path.isdir(assets):
        raise SystemExit('error: assets dir does not exist: %s' % assets)
    return assets


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('post', help='post name, e.g. int-lookup-throughput')
    ap.add_argument('filter', nargs='?', default='',
                    help='optional substring filter on chart function name')
    ap.add_argument('--blog-root', default=None,
                    help='path to the blog repo checkout (or set $BLOG_ROOT)')
    args = ap.parse_args()
    assets = resolve_assets(args.blog_root)

    js = open(os.path.join(assets, args.post + '.js')).read()
    for name, block in parse_funcs(js):
        if args.filter and args.filter not in name:
            continue
        labels, datasets = parse_block(block)
        vis = [(lab, vals) for lab, shown, vals in datasets if shown]
        # representative-size column indices
        idxs = [(n, min(range(len(labels)), key=lambda i: abs(labels[i] - n))) for n in REP_N] if labels else []
        print('\n### %s' % name)
        print('   visible %d / %d total maps-hashes' % (len(vis), len(datasets)))
        header = 'N=     ' + ' '.join('%9d' % labels[i] for _, i in idxs)
        print('   ' + header)
        # rank visible by mean value across all sizes (lower=faster)
        def meanval(vals):
            v = [x for x in vals if x > 0]
            return sum(v) / len(v) if v else 9e9
        for lab, vals in sorted(vis, key=lambda t: meanval(t[1])):
            cells = ' '.join('%9.2f' % vals[i] if i < len(vals) else '    -' for _, i in idxs)
            print('   %-42s %s' % (lab, cells))


if __name__ == '__main__':
    main()
