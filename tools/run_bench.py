import os
import sys
import re
import subprocess
import copy

# TODO: change the command prefix to what suits the platform
run_command_prefix = ['time', "taskset", "-c", "12"]


def get_work_dir_paths():
    tools_dir_path = os.path.dirname(os.path.realpath(__file__))
    root_dir_path = os.path.dirname(tools_dir_path)
    build_dir_path = os.path.join(root_dir_path, "build")
    return root_dir_path, build_dir_path


def get_exe_filepaths(build_dir_path):
    filename_list = [f for f in os.listdir(build_dir_path) if os.path.isfile(os.path.join(build_dir_path, f))]
    exe_file_pattern = re.compile(r'bench_(.+)__(.+)')
    exe_filename_list = []
    # map_name_list = []
    # hash_name_list = []
    for filename in filename_list:
        match_ret = exe_file_pattern.findall(filename)
        if match_ret:
            exe_filename_list.append(filename)
    exe_file_path_list = [os.path.join(build_dir_path, filename) for filename in exe_filename_list]
    return exe_file_path_list


def main():
    argv_len = len(sys.argv)
    if argv_len < 3:
        print("Invalid parameters!\nUsage: python3 run_bench.py seed export_data_directory")
        return
    seed = int(sys.argv[1])
    export_dir_path = sys.argv[2]
    print("Will export test data to %s" % export_dir_path)
    root_dir_path, build_dir_path = get_work_dir_paths()
    exe_file_path_list = get_exe_filepaths(build_dir_path)
    print("Will run the following tests:")
    print(exe_file_path_list)
    for exe_file_path in exe_file_path_list:
        call_arg_list = copy.deepcopy(run_command_prefix)
        call_arg_list.extend([exe_file_path, str(seed), export_dir_path])
        subprocess.run(call_arg_list)


if __name__ == '__main__':
    main()
