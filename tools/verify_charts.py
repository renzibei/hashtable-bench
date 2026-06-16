#!/usr/bin/env python3
"""Spot-check assembled blog chart .js data arrays against raw CSV columns.

The blog checkout lives in a separate repo; pass its path with --blog-root or
the BLOG_ROOT environment variable (no path is hard-coded).
"""
import argparse
import os
import re
import pandas as pd

BENCH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def arr2str(arr):
    return '[' + ','.join('%.3f' % v for v in arr) + ']'


def get_func_block(js_text, id_substr):
    """Return the text of the function whose name contains id_substr."""
    # split into function blocks
    idx = js_text.find('function ' + id_substr)
    # id_substr may be the full id; find the create() for it
    m = re.search(r'function (' + re.escape(id_substr) + r')_create\(\)', js_text)
    if not m:
        return None
    start = m.start()
    # block ends at next "function ..._create()" or end
    nxt = js_text.find('function ', m.end())
    return js_text[start: nxt if nxt != -1 else len(js_text)]


def get_data_array(func_block, label):
    """Extract the data:[...] array for dataset with given label 'map,hash'."""
    # find the label, then the following data: [...]
    li = func_block.find("label: '%s'," % label)
    if li == -1:
        return None
    di = func_block.find('data: [', li)
    if di == -1:
        return None
    de = func_block.find(']', di)
    nums = func_block[di + len('data: ['):de]
    return '[' + nums + ']'


# (post_js, full_canvas_id, label 'map,hash', csv_dir, csv_filename, csv_column)
CASES = [
    ('int-lookup-throughput.js',
     'Xeon_E_2388G_lb_mask_split_bits_uint64_t_co_uint64_t_rb___avg_hit_find_default_load_factor',
     'ska::flat_hash_map,absl::Hash', 'E2388G-202305-native',
     'ska::flat_hash_map__absl::Hash__mask_split_bits_uint64_t__uint64_t.csv',
     'avg_hit_default_load_factor_lookup_ns'),
    ('int-lookup-throughput.js',
     'M1_Max_lb_mask_split_bits_uint64_t_co_uint64_t_rb___avg_hit_find_default_load_factor',
     'ska::flat_hash_map,absl::Hash', 'm1-max-202305',
     'ska::flat_hash_map__absl::Hash__mask_split_bits_uint64_t__uint64_t.csv',
     'avg_hit_default_load_factor_lookup_ns'),
    ('int-insert-construct.js',
     'M1_Max_lb_uniform_uint64_t_co_uint64_t_rb___avg_insert_time_with_reserve',
     'tsl::robin_map,absl::Hash', 'm1-max-202305',
     'tsl::robin_map__absl::Hash__uniform_uint64_t__uint64_t.csv',
     'avg_construct_time_with_reserve_ns'),
    ('int-erase-insert.js',
     'Xeon_E_2388G_lb_mask_high_bits_uint64_t_co_uint64_t_rb___avg_erase_insert_time',
     'absl::flat_hash_map,absl::Hash', 'E2388G-202305-native',
     'absl::flat_hash_map__absl::Hash__mask_high_bits_uint64_t__uint64_t.csv',
     'avg_erase_insert_ns'),
    ('int-iterate.js',
     'M1_Max_lb_mask_low_bits_uint64_t_co_uint64_t_rb___avg_iterate',
     'absl::node_hash_map,absl::Hash', 'm1-max-202305',
     'absl::node_hash_map__absl::Hash__mask_low_bits_uint64_t__uint64_t.csv',
     'avg_iterate_ns'),
    ('12-byte-string-lookup.js',
     'Xeon_E_2388G_lb_small_string_fix_12_co_uint64_t_rb___avg_hit_find_default_load_factor',
     'emhash::hash_map7,xxHash_xxh3', 'E2388G-202305-native',
     'emhash::hash_map7__xxHash_xxh3__small_string_fix_12__uint64_t.csv',
     'avg_hit_default_load_factor_lookup_ns'),
    ('64-byte-string-lookup.js',
     'M1_Max_lb_long_string_max_64_co_uint64_t_rb___avg_miss_find_default_load_factor',
     'ska::flat_hash_map,xxHash_xxh3', 'm1-max-202305',
     'ska::flat_hash_map__xxHash_xxh3__long_string_max_64__uint64_t.csv',
     'avg_miss_default_load_factor_lookup_ns'),
    ('memory-usage-and-load-factor.js',
     'M1_Max_lb_mask_split_bits_uint64_t_co_uint64_t_rb___heap_memory_size',
     'absl::flat_hash_map,absl::Hash', 'm1-mem',
     'absl::flat_hash_map__absl::Hash__mask_split_bits_uint64_t__uint64_t.csv',
     'final_default_load_factor_size_mb'),
    ('memory-usage-and-load-factor.js',
     'Xeon_E_2388G_lb_mask_split_bits_uint64_t_co_uint64_t_rb___load_factor_with_default_max_load_factor',
     'absl::flat_hash_map,absl::Hash', 'E2388G-202305-native',
     'absl::flat_hash_map__absl::Hash__mask_split_bits_uint64_t__uint64_t.csv',
     'no_rehash_load_factor'),
    ('int-lookup-latency.js',
     'Xeon_E_2388G_lb_mask_split_bits_uint64_t_co_uint64_t_rb__co_lookup_hit_default_load_factor_P99_latency',
     'ska::flat_hash_map,absl::Hash', 'E2388G-202305-native',
     'ska::flat_hash_map__absl::Hash__mask_split_bits_uint64_t__uint64_t.csv',
     'lookup_hit_default_load_factor_P99_latency'),
]


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
    ap.add_argument('--blog-root', default=None,
                    help='path to the blog repo checkout (or set $BLOG_ROOT)')
    args = ap.parse_args()
    assets = resolve_assets(args.blog_root)

    npass = nfail = 0
    for post_js, cid, label, csv_dir, csv_fn, col in CASES:
        js_path = os.path.join(assets, post_js)
        csv_path = os.path.join(BENCH, 'results', csv_dir, 'csv', csv_fn)
        tag = '%s | %s | %s' % (post_js, label, col)
        if not os.path.exists(js_path) or not os.path.exists(csv_path):
            print('SKIP (missing file): %s' % tag); continue
        js_text = open(js_path).read()
        block = get_func_block(js_text, cid)
        if block is None:
            print('FAIL (no func %s): %s' % (cid, tag)); nfail += 1; continue
        js_arr = get_data_array(block, label)
        if js_arr is None:
            print('FAIL (no dataset %s): %s' % (label, tag)); nfail += 1; continue
        df = pd.read_csv(csv_path, sep=',').rename(columns=lambda x: x.strip())
        if col not in df.columns:
            print('FAIL (no col %s): %s' % (col, tag)); nfail += 1; continue
        csv_arr = arr2str(list(df[col]))
        if js_arr == csv_arr:
            print('PASS: %s' % tag); npass += 1
        else:
            print('FAIL (mismatch): %s' % tag)
            print('   js : %s' % js_arr[:160])
            print('   csv: %s' % csv_arr[:160])
            nfail += 1
    print('\n%d passed, %d failed' % (npass, nfail))


if __name__ == '__main__':
    main()
