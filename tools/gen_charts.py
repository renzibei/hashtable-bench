#!/usr/bin/env python3
"""Headless chart-fragment generator for the hash table benchmark blog.

Faithful, matplotlib-free port of tools/analyze_results.ipynb. For a given
results data dir + platform display name, it reads every CSV and writes one
Chart.js fragment (.txt) per (key/value, metric) into results/<data-dir>/chart_js/.

The blog assembler (assemble_post_js.py) later stitches the per-machine
fragments into the per-post .js files.

Curation parameters preserved verbatim from the notebook:
  - show_top_k_hash_only=True, show_top_k_hash_num=1  (default-visible = best hash per map)
  - ymax thresholds (lookup+latency: string 1000/int 800; throughput find: string 400/int 200)
  - HSV color cycle + pointStyle cycle, legend localeCompare sort, log axes
  - title fixups (_hit_hit_->_hit_, _miss_hit_->_miss_)
  - get_chart_js_str is called WITHOUT use_log_yscale, i.e. JS y-axis is always
    logarithmic, exactly as the notebook produced it.

Usage:
  python3 gen_charts.py --root <repo_root> --data-dir <name> --platform <name>
"""
import argparse
import colorsys
import itertools
import os
import re

import numpy as np
import pandas as pd

# ---- module-level globals populated by build_color_marker_dicts() -----------
map_hash_to_rgb_dict = {}
hash2style_dict = {}
hash2marker_dict = {}


# ---- notebook cell 3 --------------------------------------------------------
def gen_rgb_list(color_num, start_h=0.0):
    h_list = [(start_h + i * (1.0 / (color_num))) % 1.0 for i in range(color_num)]
    rgb_list = []
    for i in range(color_num):
        h = h_list[i]
        new_color = colorsys.hsv_to_rgb(h, 0.55, 0.95)
        rgb_list.append(new_color)
    return rgb_list


# ---- notebook cells 5-9 (helpers) -------------------------------------------
def get_bg_color(red, green, blue):
    return (red + 255) // 2, (green + 255) // 2, (blue + 255) // 2


def fraction_rgb_to_int(rgb_tuple):
    return round(rgb_tuple[0] * 255.0), round(rgb_tuple[1] * 255.0), round(rgb_tuple[2] * 255.0)


def min_exclude_zero(arr):
    ret = 1e+14
    for v in arr:
        if abs(v) > 1e-8:
            ret = min(ret, v)
    if abs(ret - (1e+14)) < 1e-6:
        return 0.0
    return ret


def get_yaxis_min(min_value):
    return min_value * 0.9


def arr2str(arr):
    ret = '['
    length = len(arr)
    for i, v in enumerate(arr):
        if i + 1 < length:
            ret += '%.3f,' % v
        else:
            ret += '%.3f' % v
    ret += ']'
    return ret


def is_two_list_same(list1, list2):
    if len(list1) != len(list2):
        return False
    length = len(list1)
    for i in range(length):
        if list1[i] != list2[i]:
            return False
    return True


def encode_title(title):
    title = title.replace('<', "lb_")
    title = title.replace('>', "_rb_")
    title = title.replace(',', "_co_")
    title = title.replace('%', 'percent')
    title = title.replace('-', '_')
    return title


def decode_title(title):
    # use '{' instead of '<' because '<' caused strange bugs when rendering
    title = title.replace('lb_', r'{')
    title = title.replace('_rb_', r'}')
    title = title.replace('_co_', ',')
    title = title.replace('percent', '%')
    return title


def get_chart_js_str(title, x_arr_list, y_arr_list, label_list, y_axis_label, ymax=None,
                     show_top_k_hash_only=False, show_top_k_map_hash_dict=None, use_log_yscale=True):
    ret_str = ''
    ret_str += "\n\n<!--start chart.js code for " + title + "-->\n"
    title = encode_title(title)
    ret_str += "\n<!--start canvas for " + title + "-->\n"
    ret_str += "\n\n<div class=\"chart-js-outer\"><div class=\"chart-js-inner\"><canvas id=\"%s_chart\"></canvas></div></div>\n\n" % (title)
    ret_str += "\n<!--end canvas for " + title + "-->\n\n"
    global_callback_arr_name = "create_chart_funcs"
    function_start_str = "function %s_create() {\n" % (title)
    add_to_callback_arr_str = "%s.push(async() => {%s_create();});\n" % (global_callback_arr_name, title)
    arr_num = len(x_arr_list)
    for i in range(1, arr_num):
        assert(is_two_list_same(x_arr_list[0], x_arr_list[i]))
    x_labels_str = "const %s_labels = " % (title)
    element_arr = x_arr_list[0]
    x_labels_str += str(list(element_arr)) + ";\n"
    datasets_str = "datasets: [\n"
    point_radius = "chart_js_point_r"
    point_hover_radius = 12
    min_value_list = []
    for i in range(arr_num):
        map_hash_name = label_list[i]
        map_name, hash_name = map_hash_name.split(',')
        y_axis_min = min_exclude_zero(y_arr_list[i])
        line_r, line_g, line_b = fraction_rgb_to_int(map_hash_to_rgb_dict[map_hash_name])
        bg_r, bg_g, bg_b = get_bg_color(line_r, line_g, line_b)
        hidden_flag = 'false'
        min_value_list.append(y_axis_min)
        if show_top_k_hash_only:
            if show_top_k_map_hash_dict[map_hash_name] == False:
                hidden_flag = 'true'
        if ymax is not None:
            if np.max(y_arr_list[i]) >= ymax:
                hidden_flag = 'true'
        dataset_str = "{\nlabel: '%s',\nbackgroundColor: 'rgba(%d,%d,%d,0.5)',\nborderColor: 'rgb(%d,%d,%d)',\npointStyle: '%s',\npointRadius:%s,\npointHoverRadius:%d,\nhidden: %s,\ndata: " % (
            label_list[i], bg_r, bg_g, bg_b, line_r, line_g, line_b, hash2style_dict[hash_name], point_radius, point_hover_radius, hidden_flag
        )
        dataset_str += arr2str(list(y_arr_list[i])) + ",\n},\n"
        datasets_str += dataset_str
    y_axis_min = min_exclude_zero(min_value_list)
    y_axis_min = get_yaxis_min(y_axis_min)
    datasets_str += "],\n"
    data_config_str = "const %s_data = {\nlabels: %s_labels,\n%s};\n" % (title, title, datasets_str)
    title_str = "const %s_title = {\ntext: '%s',\ndisplay: true,\n};\n" % (title, decode_title(title))
    y_scale_type = 'linear'
    if use_log_yscale:
        y_scale_type = 'logarithmic'
    scales_str = "const %s_scales = {\nx: {display: true, type: 'logarithmic',title: {text : 'element num', display: true,},},y: {min: %.3f, display: true,type: '%s',title: {text : '%s',display: true,},}};" % (title, y_axis_min, y_scale_type, y_axis_label)
    options_str = "const %s_options = {\nmaintainAspectRatio: false,\nscales: %s_scales,\nplugins:{\ntitle: %s_title,\nlegend: {\nlabels: {\nusePointStyle: true,sort: function(a,b) {return a.text.localeCompare(b.text);},},\n},\n},\n};" % (title, title, title)
    config_str = "const %s_config = {\n type: 'line',\ndata: %s_data,\noptions: %s_options,\n};\n" % (title, title, title)
    new_chart_str = "new Chart( document.getElementById('%s_chart'), %s_config,);" % (title, title)
    function_end_str = "};\n"
    ret_str += "\n\n<!-- Add to call back script -->\n<script>\n\n\n" + add_to_callback_arr_str + "\n\n</script>\n\n"
    ret_str += ("\n<script>\n\n\n/* Start of data js for %s */\n" % title) + function_start_str + x_labels_str + data_config_str + title_str + scales_str + options_str + config_str + new_chart_str + function_end_str + ("\n/* End of data js for %s */\n\n\n</script>\n" % title)
    ret_str += "<!--end chart.js code for " + title + "-->\n\n"
    return ret_str


# ---- notebook cell 10 (rank helpers for top-k) ------------------------------
def get_mat_sort_pos(mat):
    sorted_index_arr = np.argsort(mat, axis=0)
    sort_pos_arr = np.zeros(shape=mat.shape, dtype=np.uint64)
    for j in range(sorted_index_arr.shape[1]):
        for i in range(sorted_index_arr.shape[0]):
            sort_pos_arr[sorted_index_arr[i][j]][j] = i
    return sort_pos_arr


def get_arr_sort_pos(arr):
    sorted_index_arr = np.argsort(arr)
    sort_pos_arr = np.zeros(shape=(len(arr)), dtype=np.uint64)
    for i in range(len(sorted_index_arr)):
        sort_pos_arr[sorted_index_arr[i]] = i
    return sort_pos_arr


# ---- headless replacement for notebook cell 11 (plot_lines) -----------------
# Only the curation logic is kept; all matplotlib/PNG code is dropped. The JS is
# written exactly as the notebook wrote it (get_chart_js_str called without
# use_log_yscale, hence always logarithmic).
def write_chart_js(title, x_arr_list, y_arr_list, label_list, y_axis_label,
                   export_chartjs_dir_path, ymax=None,
                   show_top_k_hash_only=True, top_k_hash_num=1):
    # fix some typos of the existing data
    title = title.replace('_hit_hit_', '_hit_')
    title = title.replace('_miss_hit_', '_miss_')
    arr_num = len(x_arr_list)
    assert(len(x_arr_list) == len(y_arr_list))

    show_map_hash_dict = {}
    if show_top_k_hash_only:
        map_data_index_dict = {}
        for i in range(arr_num):
            map_name, hash_name = label_list[i].split(",")
            if map_name not in map_data_index_dict:
                map_data_index_dict[map_name] = []
            map_data_index_dict[map_name].append(i)
        temp_data_mat = np.array(y_arr_list)
        for map_name, data_index_list in map_data_index_dict.items():
            data_arr_list = temp_data_mat[data_index_list]
            data_mat = np.array(data_arr_list)
            for i in range(data_mat.shape[0]):
                for j in range(data_mat.shape[1]):
                    if data_mat[i][j] == 0.0:
                        data_mat[i][j] = 1e+9
            sort_pos_mat = get_mat_sort_pos(data_mat)
            avg_pos_array = np.mean(sort_pos_mat, axis=1)
            avg_pos_sort_pos_arr = get_arr_sort_pos(avg_pos_array)
            for i in range(avg_pos_sort_pos_arr.shape[0]):
                avg_pos_sort_pos = avg_pos_sort_pos_arr[i]
                map_hash_name = label_list[data_index_list[i]]
                if avg_pos_sort_pos < top_k_hash_num:
                    show_map_hash_dict[map_hash_name] = True
                else:
                    show_map_hash_dict[map_hash_name] = False

    # ymax defaulting (verbatim from notebook lines 402-413)
    if ymax is None:
        if ('lookup' in title) and ('latency' in title):
            ymax = 1000 if 'string' in title else 800
        elif ('lookup' in title) or ('find' in title):
            ymax = 400 if 'string' in title else 200

    export_chart_js_filename = title + ".txt"
    export_chart_js_path = os.path.join(export_chartjs_dir_path, export_chart_js_filename)
    with open(export_chart_js_path, "w") as out_chart_js_f:
        # NOTE: use_log_yscale intentionally NOT passed, matching the notebook
        chart_js_str = get_chart_js_str(
            title, x_arr_list, y_arr_list, label_list, y_axis_label, ymax=ymax,
            show_top_k_hash_only=show_top_k_hash_only, show_top_k_map_hash_dict=show_map_hash_dict)
        out_chart_js_f.write(chart_js_str)


# ---- notebook cell 4 (CSV discovery + color/marker dicts) -------------------
def build_k_v_dict_and_styles(cur_csv_dir_path, platform_name):
    global map_hash_to_rgb_dict, hash2style_dict, hash2marker_dict
    filename_list = [f for f in os.listdir(cur_csv_dir_path) if os.path.isfile(os.path.join(cur_csv_dir_path, f))]
    data_filename_pattern = re.compile(r'(.+)__(.+)__(.+)__(.+)\.csv')
    map_name_list = []
    hash_name_list = []
    k_v_dict = {}
    for filename in filename_list:
        match_ret = data_filename_pattern.findall(filename)
        if match_ret:
            data_file_path = os.path.join(cur_csv_dir_path, filename)
            temp_map_name = match_ret[0][0]
            temp_hash_name = match_ret[0][1]
            map_name_list.append(temp_map_name)
            hash_name_list.append(temp_hash_name)
            temp_k_v_name = platform_name + '_' + '<' + match_ret[0][2] + ',' + match_ret[0][3] + '>'
            if temp_k_v_name not in k_v_dict:
                k_v_dict[temp_k_v_name] = {'data_filepath_list': [], 'hash_name_list': [], 'map_name_list': []}
            k_v_dict[temp_k_v_name]['data_filepath_list'].append(data_file_path)
            k_v_dict[temp_k_v_name]['hash_name_list'].append(temp_hash_name)
            k_v_dict[temp_k_v_name]['map_name_list'].append(temp_map_name)
    print("total %d data files" % (len(map_name_list)))
    map_name_list = sorted(set(map_name_list))
    hash_name_list = sorted(set(hash_name_list))
    map_hash_list = []
    for map_name in map_name_list:
        for hash_name in hash_name_list:
            map_hash_list.append(map_name + "," + hash_name)
    map_hash_name_num = len(map_hash_list)
    map_num = len(map_name_list)
    hash_num = len(hash_name_list)
    rgb_list = gen_rgb_list(map_hash_name_num * 10, 2 * 1.0 / map_num)
    map_hash_to_rgb_dict = {}
    for i, map_name in enumerate(map_name_list):
        for j, hash_name in enumerate(hash_name_list):
            use_rgb = rgb_list[i * hash_num * 10 + j * 8]
            map_hash_to_rgb_dict[map_hash_list[i * hash_num + j]] = use_rgb
    marker_iter = itertools.cycle(('^', 'o', 's', 'x', 'p', '<', '*', '>', '+', 'v', 'D'))
    point_style_iter = itertools.cycle(('circle', 'rect', 'triangle', 'rectRot', 'rectRounded', 'cross', 'crossRot', 'star'))
    hash2marker_dict = {}
    hash2style_dict = {}
    for hash_name in hash_name_list:
        hash2marker_dict[hash_name] = next(marker_iter)
        hash2style_dict[hash_name] = next(point_style_iter)
    return k_v_dict


# ---- notebook cell 12 (per-k_v metric loop) ---------------------------------
def generate_all(k_v_dict, export_chartjs_dir_path):
    show_top_k_hash_only = True
    show_top_k_hash_num = 1
    n_written = 0
    for k_v_name, data_dict in k_v_dict.items():
        data_file_num = len(data_dict['data_filepath_list'])
        map_hash_name_list = []
        element_num_arr_list = []
        construct_time_with_reserve_arr_list = []
        construct_time_without_reserve_arr_list = []
        load_factor_without_rehash_arr_list = []
        load_factor_with_rehash_arr_list = []
        erase_insert_arr_list = []
        hit_without_rehash_lookup_arr_list = []
        miss_without_rehash_lookup_arr_list = []
        may_without_rehash_lookup_arr_list = []
        hit_with_rehash_lookup_arr_list = []
        miss_with_rehash_lookup_arr_list = []
        may_with_rehash_lookup_arr_list = []
        iterate_arr_list = []
        with_final_rehash_construct_arr_list = []
        final_default_load_factor_size_mb_arr_list = []
        final_large_load_factor_size_mb_arr_list = []
        latency_arr_list_dict = {}
        for t in range(data_file_num):
            temp_map_hash_name = data_dict['map_name_list'][t] + "," + data_dict['hash_name_list'][t]
            map_hash_name_list.append(temp_map_hash_name)
            temp_data_df = pd.read_csv(data_dict['data_filepath_list'][t], sep=',')
            temp_data_df = temp_data_df.rename(columns=lambda x: x.strip())
            column_name_list = temp_data_df.columns.to_list()
            latency_item_pattern = re.compile(r'.+_latency')
            for column_name in column_name_list:
                if latency_item_pattern.match(column_name) is not None:
                    new_column_name = column_name.replace('_miss_hit_', '_miss_')
                    if new_column_name not in latency_arr_list_dict:
                        latency_arr_list_dict[new_column_name] = ([], [], [])
                    latency_arr_list_dict[new_column_name][0].append(temp_data_df['element_num'])
                    latency_arr_list_dict[new_column_name][1].append(temp_data_df[column_name])
                    latency_arr_list_dict[new_column_name][2].append(temp_map_hash_name)
            element_num_arr_list.append(temp_data_df['element_num'])
            construct_time_with_reserve_arr_list.append(temp_data_df['avg_construct_time_with_reserve_ns'])
            construct_time_without_reserve_arr_list.append(temp_data_df['avg_construct_time_without_reserve_ns'])
            erase_insert_arr_list.append(temp_data_df['avg_erase_insert_ns'])
            load_factor_without_rehash_arr_list.append(temp_data_df['no_rehash_load_factor'])
            load_factor_with_rehash_arr_list.append(temp_data_df['with_rehash_load_factor'])
            hit_without_rehash_lookup_arr_list.append(temp_data_df['avg_hit_default_load_factor_lookup_ns'])
            miss_without_rehash_lookup_arr_list.append(temp_data_df['avg_miss_default_load_factor_lookup_ns'])
            may_without_rehash_lookup_arr_list.append(temp_data_df['avg_50%_hit_default_load_factor_lookup_ns'])
            hit_with_rehash_lookup_arr_list.append(temp_data_df['avg_hit_large_max_load_factor_lookup_ns'])
            miss_with_rehash_lookup_arr_list.append(temp_data_df['avg_miss_large_max_load_factor_lookup_ns'])
            may_with_rehash_lookup_arr_list.append(temp_data_df['avg_50%_hit_large_max_load_factor_lookup_ns'])
            iterate_arr_list.append(temp_data_df['avg_iterate_ns'])
            if 'final_default_load_factor_size_mb' in temp_data_df:
                final_default_load_factor_size_mb_arr_list.append(temp_data_df['final_default_load_factor_size_mb'])
            if 'final_large_load_factor_size_mb' in temp_data_df:
                final_large_load_factor_size_mb_arr_list.append(temp_data_df['final_large_load_factor_size_mb'])
            if 'avg_with_final_rehash_construct_ns' in temp_data_df:
                with_final_rehash_construct_arr_list.append(temp_data_df['avg_with_final_rehash_construct_ns'])

        def emit(suffix, y_list, y_label, **kw):
            write_chart_js(k_v_name + suffix, element_num_arr_list, y_list, map_hash_name_list,
                           y_label, export_chartjs_dir_path,
                           show_top_k_hash_only=show_top_k_hash_only, top_k_hash_num=show_top_k_hash_num, **kw)

        emit('__avg_insert_time_with_reserve', construct_time_with_reserve_arr_list, 'ns')
        emit('__avg_insert_time_without_reserve', construct_time_without_reserve_arr_list, 'ns')
        emit('__avg_erase_insert_time', erase_insert_arr_list, 'ns')
        emit('__avg_hit_find_default_load_factor', hit_without_rehash_lookup_arr_list, 'ns')
        emit('__avg_miss_find_default_load_factor', miss_without_rehash_lookup_arr_list, 'ns')
        emit(r'__avg_50%_hit_find_default_load_factor', may_without_rehash_lookup_arr_list, 'ns')
        emit('__avg_hit_find_large_max_load_factor', hit_with_rehash_lookup_arr_list, 'ns')
        emit('__avg_miss_find_large_max_load_factor', miss_with_rehash_lookup_arr_list, 'ns')
        emit(r'__avg_50%_hit_find_large_max_load_factor', may_with_rehash_lookup_arr_list, 'ns')
        emit('__avg_iterate', iterate_arr_list, 'ns')
        if len(with_final_rehash_construct_arr_list) == data_file_num:
            emit('__avg_construct_time_with_final_rehash', with_final_rehash_construct_arr_list, 'ns')
        emit('__load_factor_with_default_max_load_factor', load_factor_without_rehash_arr_list, 'load_factor')
        emit('__load_factor_with_large_max_load_factor', load_factor_with_rehash_arr_list, 'load_factor')
        if len(final_default_load_factor_size_mb_arr_list) > 0:
            emit('__heap_memory_size', final_default_load_factor_size_mb_arr_list, 'MB')
        if len(final_large_load_factor_size_mb_arr_list) > 0:
            emit('__heap_memory_size_large_max_load_factor', final_large_load_factor_size_mb_arr_list, 'MB')
        for latency_item, data_tuple in latency_arr_list_dict.items():
            (temp_ele_arr_list, data_arr_list, temp_map_hash_name_list) = data_tuple
            write_chart_js(k_v_name + ',' + latency_item, temp_ele_arr_list, data_arr_list,
                           temp_map_hash_name_list, 'ns', export_chartjs_dir_path,
                           show_top_k_hash_only=show_top_k_hash_only, top_k_hash_num=show_top_k_hash_num)
        n_written += 1
    print("processed %d key/value groups" % n_written)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                    help='benchmark repo root (contains results/)')
    ap.add_argument('--data-dir', required=True, help='results/<data-dir> to read CSVs from')
    ap.add_argument('--platform', required=True, help='platform display name, e.g. M1-Max / Xeon-E-2388G')
    args = ap.parse_args()

    cur_data_dir_path = os.path.join(args.root, 'results', args.data_dir)
    cur_csv_dir_path = os.path.join(cur_data_dir_path, 'csv')
    export_chartjs_dir_path = os.path.join(cur_data_dir_path, 'chart_js')
    os.makedirs(export_chartjs_dir_path, exist_ok=True)
    print("data-dir=%s platform=%s" % (args.data_dir, args.platform))
    print("csv: %s" % cur_csv_dir_path)
    print("out: %s" % export_chartjs_dir_path)

    k_v_dict = build_k_v_dict_and_styles(cur_csv_dir_path, args.platform)
    print("key/value groups: %s" % list(k_v_dict.keys()))
    generate_all(k_v_dict, export_chartjs_dir_path)


if __name__ == '__main__':
    main()
