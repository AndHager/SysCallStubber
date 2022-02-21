#!/usr/bin/python3
import argparse
import os
import re
import math
import sys
import matplotlib.pyplot as plt

### -------------
# ---- Utils ----
### -------------
def get_x(size, exec_t):
    return [
       x * exec_t/size
       for x in range(size) 
    ]

def sum_lists(first, second):
    if len(first) > len(second):
        longer = first
        shorter = second
    else:
        longer = second
        shorter = first
    result = longer.copy()
    for i in range(len(shorter)):
        result[i] += shorter[i]
    return result

def get_mean(data):
    mean = []
    for points in data:
        mean = sum_lists(mean, points)
    return [
        mean[i]/len(data)
        for i in range(len(mean))
    ]

### ---------------------
# ---- Visualization ----
### ---------------------
def align_data(first, second):
    isFirstShorter = len(first) < len(second)
    if isFirstShorter:
        longer = second
        shorter = first
    else: 
        longer = first
        shorter = second

    min_size = len(shorter)
    max_size = len(longer)

    stratch_factor = max_size/min_size - 1
    sv_floor = math.floor(stratch_factor)
    index = 0
    for i in range(min_size):
        if stratch_factor > 1:
            val = shorter[index]
            for _ in range(sv_floor):
                index += 1
                shorter.insert(index, val)
        index += 1

    for _ in range(10):
        min_size = len(shorter)
        max_size = len(longer)

        stratch_factor = max_size/min_size - 1
        index = 0
        for i in range(1, min_size):
            if 0.005 < stratch_factor < 1:
                inv_stratch_factor = math.floor(1/stratch_factor + 1) 
                if i % inv_stratch_factor == 0:
                    val = shorter[index]
                    index += 1
                    shorter.insert(index, val)
            index += 1

    for i in range(len(shorter), max_size):
        shorter.insert(i-1, shorter[i-1])
    
    assert len(shorter) == len(longer)

    if isFirstShorter:
        return shorter, longer
    else: 
        return longer, shorter


def add_diff_axis_lables():
    axes = plt.gca()
    axes.set_xscale('linear')
    axes.set_yscale('linear')

    plt.ylabel('diff in %')
    plt.xlabel('t in sec')

def add_axis_lables(absolute):
    axes = plt.gca()
    axes.set_xscale('linear')
    axes.set_yscale('linear')
    plt.xlabel('t in sec')
    if absolute: 
        y_label =' exec total'
    else:
        y_label =' exec/sec'
    plt.ylabel(y_label)

def visualize(data, exec_t, show_data_points):
    if show_data_points:
        for points in data:
            plt.scatter(get_x(len(points), exec_t), points, s=0.25, alpha=0.4)

    mean = get_mean(data)
    size = len(mean)
    x_vals = get_x(size, exec_t)
    plt.plot(x_vals, mean)

def visualize_individual_fuzzer(normal_data, data, exec_t, keys, prefix, absolute, path):
    print(f'__INFO__ line count in {prefix:12}:')
    for index in range(len(keys)):
        key = keys[index][5:]
        normal_len = len(normal_data[index])
        data_len = len(data[index])
        abs_diff = normal_len - data_len
        percent_diff = 100 - data_len * 100 / normal_len
        print(f'\t{key:30}: normal : {normal_len:6}, {prefix:12}: {data_len:6}, abs-diff : {abs_diff:6}, %-diff : {percent_diff:6.2f} %')
        if normal_len == 0 or data_len == 0:
            continue
        normal_datas, other_data = align_data(
            normal_data[index], 
            data[index]
        )

        assert len(normal_datas) == len(other_data)
        x_n = get_x(len(normal_datas), exec_t)
        plt.plot(x_n, normal_datas)
        x_o = get_x(len(other_data), exec_t)
        plt.plot(x_o, other_data)

        plt.title(key)
        add_axis_lables(absolute)
        plt.legend([
            'normal',
            prefix
        ])
        plt.savefig(path + '/' + prefix + '_'  + key + '.png', dpi=500)
        plt.close()


def visualize_var(data, exec_t):
    size = sys.maxsize
    for datas in data:
        size = min(size, len(datas))
    assert size > 0 and size < sys.maxsize
    box_count = 5
    assert box_count > 0
    box_index = [
        math.floor(pos*(size-1)/(box_count))
        for pos in range(0, box_count + 1)
    ]
    D = [
        [ datas[index] for datas in data ]
        for index in box_index
    ]
    box_pos = [
        math.floor(pos*(exec_t-1)/(box_count))
        for pos in range(0, box_count + 1)
    ]
    plt.boxplot(D, positions=box_pos)

def visualize_diff(normal_data, other_data, exec_t):
    normal_mean = get_mean(normal_data)
    other_mean = get_mean(other_data)

    size = min(
        len(normal_mean), 
        len(other_mean)
    )
    other_diff = [
        other_mean[i]/normal_mean[i] - 1
        for i in range(size)
        if normal_mean[i] > 0.001
    ]
    plt.plot(get_x(len(other_diff), exec_t), other_diff)

def get_last_values(datas):
    return [
        data[-1]
        for data in datas
    ]

def visualize_bar(normal_data, static_data, dynamic_data, keys, path):
    keys = [
        key[5:]
        for key in keys
    ]
    normal_vals = get_last_values(normal_data)
    static_vals = get_last_values(static_data)
    dynamic_vals = get_last_values(dynamic_data)
    size = len(normal_vals)
    assert size == len(static_vals) \
        and size == len(dynamic_vals) \
        and size == len(keys)
    X = [x for x in range(size)]
    plt.bar(X, normal_vals, width=0.25)
    X = [x + 0.25 for x in range(size)]
    plt.bar(X, static_vals, width=0.25)
    X = [x + 0.5 for x in range(size)]
    plt.bar(X, dynamic_vals, width=0.25)

    plt.legend(['Base Data', 'Static Replaced', 'Dynamic Replaced'])
    xt = [i + 0.25 for i in range(size)]
    plt.xticks(xt, keys, rotation=30)

    plt.title("Total executions")
    plt.xlabel('Fuzzer')
    plt.ylabel('Executions')

    plt.savefig(path + '/bars.png', dpi=500)
    plt.close()

### ------------------------------
# ---- Extract data from file ----
### ------------------------------
def get_datapoint_of_line(line, absolute):
    if absolute:
        pattern = '#\d+'
    else:
        pattern = 'exec/s: \d+'
    datastr = re.findall(pattern, line)
    if len(datastr) > 0:
        datapoint = re.sub('[^0-9]', '', datastr[0])
    else:
        datapoint = 0
    return float(datapoint)

def get_data_of_file(file_path, absolute):
    file = open(file_path, 'r')
    data = [
        get_datapoint_of_line(line, absolute)
        for line in file.readlines()
        if line.startswith('#') and not str(line[1:]).startswith('#')
    ]
    file.close() 
    return data

def get_data_of_dir(path, absolute):
    keys = [
        filename[:len(filename) - 11]
        for (_, _, filenames) in os.walk(path + '/out')
        for filename in filenames
        if filename.endswith('_output.txt')
    ]
    data = [
        get_data_of_file(
            os.sep.join([dirpath, filename]),  
            absolute
        )
        for (dirpath, _, filenames) in os.walk(path + '/out')
        for filename in filenames
        if filename.endswith('_output.txt')
    ]
    return data, keys

### ---------------------------------
# ---- Entrypoint of this script ----
### ---------------------------------

def main(argv):
    path = argv.path
    exec_t = argv.execution_time
    absolute = argv.absolute
    show_data_points = argv.show_data_points
    visualize_static_logging = argv.visualize_static_logging
    visualize_median = argv.visualize_median

    normal_data, normal_keys = get_data_of_dir(path + '/normal', absolute)
    static_log_data, _ = get_data_of_dir(path + '/static_log', absolute)
    static_repl_data, _ = get_data_of_dir(path + '/static_repl', absolute)
    dynamic_data, _ = get_data_of_dir(path + '/dynamic', absolute)

    # ----------------------------------------
    # ---- Visualize all in a bar diagram ----
    # ----------------------------------------
    visualize_bar(normal_data, static_repl_data, dynamic_data, normal_keys, path)

    # -----------------------
    # ---- Visualize all ----
    # -----------------------
    if visualize_static_logging:
        visualize_individual_fuzzer(
            normal_data, static_log_data, 
            exec_t, normal_keys, 
            'static_log', 
            absolute, path
        )
    visualize_individual_fuzzer(
        normal_data, static_repl_data, 
        exec_t, normal_keys, 
        'static', 
        absolute, path
    )
    visualize_individual_fuzzer(
        normal_data, dynamic_data, 
        exec_t, normal_keys, 
        'dynamic', 
        absolute, path
    )
    if visualize_median:
        visualize(normal_data, exec_t, show_data_points)
        if visualize_static_logging:
            visualize(static_log_data, exec_t, show_data_points)
        visualize(static_repl_data, exec_t, show_data_points)
        visualize(dynamic_data, exec_t, show_data_points)

        visualize_var(normal_data, exec_t)
        if visualize_static_logging:
            visualize_var(static_log_data, exec_t)
        visualize_var(static_repl_data, exec_t)
        visualize_var(dynamic_data, exec_t)

        plt.title('Performance analysis')
        add_axis_lables(absolute, )
        plt.legend([
            'normal', 
            'static log', 
            'static replaced', 
            'dynamic'
        ])
        plt.savefig(path + '/mean_data_all.png', dpi=500)
        plt.close()

        # ----------------------------------
        # ---- Visualize static logging ----
        # ----------------------------------
        if visualize_static_logging:
            visualize(normal_data, exec_t, show_data_points)
            visualize(static_log_data, exec_t, show_data_points)

            visualize_var(normal_data, exec_t)
            visualize_var(static_log_data, exec_t)

            plt.title('Static Logging')
            add_axis_lables(absolute, )
            plt.legend([
                'normal', 
                'static log'
            ])    
            plt.savefig(path + '/mean_data_static_log.png', dpi=500)
            plt.close()

        # -----------------------------------
        # ---- Visualize static replaced ----
        # -----------------------------------
        visualize(normal_data, exec_t, show_data_points)
        visualize(static_repl_data, exec_t, show_data_points)

        visualize_var(normal_data, exec_t)
        visualize_var(static_repl_data, exec_t)

        plt.title('Static Replaced')
        add_axis_lables(absolute, )
        plt.legend([
            'normal', 
            'static replaced'
        ])
        plt.savefig(path + '/mean_data_static_repl.png', dpi=500)
        plt.close()

        # ---------------------------
        # ---- Visualize dynamic ----
        # ---------------------------
        visualize(normal_data, exec_t, show_data_points)
        visualize(dynamic_data, exec_t, show_data_points)

        visualize_var(normal_data, exec_t)
        visualize_var(dynamic_data, exec_t)
        
        plt.title('DynamicReplaced')
        add_axis_lables(absolute, )
        plt.legend([
            'normal', 
            'dynamic'
        ])    
        plt.savefig(path + '/mean_data_dynamic.png', dpi=500)
        plt.close()

        # -----------------------------------
        # ---- Visualize all differneces ----
        # -----------------------------------
        if visualize_static_logging:
            visualize_diff(normal_data, static_log_data, exec_t)
        visualize_diff(normal_data, static_repl_data, exec_t)
        visualize_diff(normal_data, dynamic_data, exec_t)

        plt.title('DynamicReplaced')
        add_diff_axis_lables()
        legend = ['static replaced', 'dynamic']
        if visualize_static_logging:
            legend = ['static log'] + legend
        plt.legend(
            legend,
            framealpha=0.5, 
            loc='upper left', 
            prop={'size': 6}
        )
        plt.savefig(path + '/mean_diff_all.png', dpi=500)
        plt.close()

if __name__ == '__main__':
    # Setup script argument parser
    parser = argparse.ArgumentParser(
        description='Perforance evaluations of the provided fuzzing logs.'
    )
    parser.add_argument(
        'path', 
        metavar='PATH',
        type=str,
        help='Path to the folder containing the fuzzing logs.'
    )
    parser.add_argument(
        '-et',  '--execution-time',
        metavar='EXECUTION_TIME',
        type=int,
        help='Execution time the fuzzer in sec.'
    )
    parser.add_argument(
        '-a',  '--absolute',
        metavar='ABSOLUTE',
        type=bool,
        default=True,
        help='Decide if you want to evaluate the absolute value of the fuzzing input executions or the exec/s.'
    )
    parser.add_argument(
        '-vsl',  '--visualize-static-logging',
        metavar='VISUALIZE_STATIC_LOGGING',
        type=bool,
        default=False,
        help='Decide if you want visualize the output of the static logging fuzzer run (not recomended due to different execution time).'
    )
    parser.add_argument(
        '-vm',  '--visualize-median',
        metavar='VISUALIZE_MEDIAN',
        type=bool,
        default=False,
        help='Decide if you want visualize the median of all fuzzer (not recomended due to different fuzzer).'
    )
    parser.add_argument(
        '--show_data_points',
        metavar='SHOW_DATA_POINTS',
        type=bool,
        default=False,
        help='Decide if you want to want to show every data point. WARNING: leeds to a catoic scatter plot.'
    )
    argv = parser.parse_args()
    main(argv)
