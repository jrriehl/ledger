#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import csv
import subprocess
import numpy as np
from glob import glob
from shutil import copy
from datetime import date

# csv files to be generated by vm_benchmarks
vm_benchmark_path = '../../cmake-build-release/libs/vm-modules/benchmark/'
vm_benchmark_file = 'vm_benchmark.csv'
opcode_list_file = 'opcode_lists.csv'
output_path = 'results/' + date.today().strftime("%Y-%m-%d") + '/'

# number of benchmark repetitions
run_benchmarks = True
make_plots = True
make_tables = True
save_results = True
save_figures = True
n_reps = 100
imgformat = 'png'

# selectively suppress benchmarks by setting environment variables
#os.environ['FETCH_VM_BENCHMARK_NO_BASIC'] = '1'
#os.environ['FETCH_VM_BENCHMARK_NO_OBJECT'] = '1'
#os.environ['FETCH_VM_BENCHMARK_NO_PRIM_OPS'] = '1'
#os.environ['FETCH_VM_BENCHMARK_NO_MATH'] = '1'
#os.environ['FETCH_VM_BENCHMARK_NO_ARRAY'] = '1'
#os.environ['FETCH_VM_BENCHMARK_NO_TENSOR'] = '1'
#os.environ['FETCH_VM_BENCHMARK_NO_CRYPTO'] = '1'


# return index of benchmark from name listed in vm_benchmark_file
def index(row):
    return int(row[0].split('_')[0].split('/')[1])


# get the list of opcodes that are in ops but not in base_ops
def get_net_opcodes(ops, base_ops):
    base_copy = base_ops.copy()
    return [x for x in ops if x not in base_copy or base_copy.remove(x)]


def linear_fit(bms, bm_type):

    x = [bm['param_val']
         for bm in bms.values() if bm['type'] == bm_type]
    y = [bm['net_mean'] for bm in bms.values() if bm['type'] == bm_type]
    return np.polyfit(np.array(x, dtype=float), np.array(y), 1).tolist()


def aggregate_stats(bms, bm_type, base_inds):

    inds = [ind for (ind, bm) in bms.items() if bm['type'] == bm_type]
    n_type = len(inds)
    base_ind = base_inds[inds[0]]
    means = np.array([bms[ind]['mean'] for ind in inds])
    stddevs = np.array([bms[ind]['stddev'] for ind in inds])
    agg_mean = np.mean(means)
    agg_stddev = np.sqrt(np.mean(stddevs**2 + (means - agg_mean)**2))
    agg_net_mean = agg_mean - bms[base_ind]['mean']
    agg_net_stderr = np.sqrt((agg_stddev ** 2) / (n_reps*n_type) +
                             (bms[base_ind]['stddev'] ** 2) / n_reps) ** 0.5
    return agg_net_mean, agg_net_stderr


# delete old file if it exists
if run_benchmarks and os.path.exists(opcode_list_file):
    os.remove(opcode_list_file)

# run benchmarks
if run_benchmarks:
    stdout = subprocess.call([vm_benchmark_path + 'vm-benchmarks',
                              '--benchmark_out=' + vm_benchmark_file,
                              '--benchmark_out_format=csv',
                              '--benchmark_repetitions=' + str(n_reps),
                              '--benchmark_report_aggregates_only=true',
                              '--benchmark_display_aggregates_only=true'])

# read opcode lists and baseline bms
with open(opcode_list_file) as csvfile:
    csvreader = csv.reader(csvfile)
    opcode_rows = [row for row in csvreader]

# read results in text format
with open(vm_benchmark_file) as csvfile:
    csvreader = csv.reader(csvfile)
    bm_rows = [row for row in csvreader if 'Benchmark' in row[0]]

# create dicts from benchmark indices to names, baseline indices, and opcode lists
bm_names = {int(row[0]): row[1] for row in opcode_rows}
bm_inds = {row[1]: int(row[0]) for row in opcode_rows}
baseline_inds = {bm_inds[row[1]]: bm_inds[row[2]] for row in opcode_rows}
opcodes = {int(row[0]): [int(op) for (i, op) in enumerate(row[3:])]
           for row in opcode_rows}

# get cpu time stats (in ns) for each bm index
means = {index(row): float(row[3]) for row in bm_rows if 'mean' in row[0]}
medians = {index(row): float(row[3]) for row in bm_rows if 'median' in row[0]}
stddevs = {index(row): float(row[3]) for row in bm_rows if 'stddev' in row[0]}

bm_classes = ['Basic', 'String', 'Prim', 'Math', 'Array', 'Tensor', 'Sha256']
prim_bm_classes = ['Prim', 'Math']
param_bm_classes = ['String', 'Array', 'Sha256']

# collect benchmark data and stats
benchmarks = {ind: {'name': name,
                    'mean': means[ind],
                    'median': medians[ind],
                    'stddev': stddevs[ind],
                    'baseline': bm_names[baseline_inds[ind]],
                    'opcodes': opcodes[ind],
                    'net_mean': means[ind] - means[baseline_inds[ind]],
                    'net_stderr': (stddevs[ind] ** 2 + stddevs[baseline_inds[ind]] ** 2) ** 0.5 / n_reps ** 0.5,
                    'net_opcodes': get_net_opcodes(opcodes[ind], opcodes[baseline_inds[ind]]),
                    'ext_opcodes': get_net_opcodes(opcodes[baseline_inds[ind]], opcodes[ind]),
                    'type': ''
                    } for (ind, name) in bm_names.items()}

for (ind, bm) in benchmarks.items():
    bm_class = [cl for cl in bm_classes if cl in bm['name']]
    if len(bm_class) == 0:
        bm['class'] = 'Basic'
    else:
        bm['class'] = bm_class[0]
    if bm['class'] == 'Tensor':
        bm['type'] = bm['name'].split('-')[0]
        dim_size = bm['name'].split('_')[1]
        bm['dim'] = int(dim_size.split('-')[0])
        bm['param_val'] = int(dim_size.split('-')[1])**bm['dim']
    elif bm['class'] in param_bm_classes and '_' in bm['name']:
        bm['type'] = bm['name'].split('_')[0]
        bm['param_val'] = int(bm['name'].split('_')[1])

# collect tensor benchmark data
tensor_bm_types = {bm['type']
                   for bm in benchmarks.values() if bm['class'] == 'Tensor'}

# collect parameterized benchmark data
param_bm_types = {bm['type'] for bm in benchmarks.values(
) if bm['class'] in param_bm_classes and '_' in bm['name']}
param_bm_types = param_bm_types.union(tensor_bm_types)

param_bms = {type_name: {'inds': [ind for (ind, bm) in benchmarks.items() if bm['type'] == type_name],
                         'lfit': linear_fit(benchmarks, type_name),
                         'net_mean': aggregate_stats(benchmarks, type_name, baseline_inds)[0],
                         'net_stderr': aggregate_stats(benchmarks, type_name, baseline_inds)[1]}
             for type_name in param_bm_types}

for (ind, param_bm) in param_bms.items():
    ind0 = param_bm['inds'][0]
    param_bm['opcodes'] = benchmarks[ind0]['opcodes']
    param_bm['baseline'] = benchmarks[ind0]['baseline']
    param_bm['net_opcodes'] = benchmarks[ind0]['net_opcodes']
    param_bm['param_vals'] = [benchmarks[ind]['param_val']
                              for ind in param_bm['inds']]
    param_bm['net_means'] = [benchmarks[ind]['net_mean']
                             for ind in param_bm['inds']]
    param_bm['net_stderrs'] = [benchmarks[ind]['net_stderr']
                               for ind in param_bm['inds']]

if save_results:

    if not os.path.exists('results'):
        os.mkdir('results')

    if not os.path.exists(output_path):
        os.mkdir(output_path)
        os.mkdir(output_path + 'plots/')

    # copy output of benchmark runs
    copy(vm_benchmark_file, output_path + vm_benchmark_file)
    copy(opcode_list_file, output_path + opcode_list_file)

    # copy source code
    for file in glob(r'*.py'):
        copy(file, output_path)

if make_tables:

    from vm_benchmark_tables import benchmark_table, primitive_table, linear_fit_table

    for bm_cls in bm_classes:
        benchmark_table(benchmarks, n_reps, bm_cls)

    for bm_cls in prim_bm_classes:
        primitive_table(benchmarks, n_reps, bm_cls)

    for bm_cls in param_bm_classes:
        linear_fit_table(param_bms, n_reps, bm_cls)

if make_plots:

    from vm_benchmark_plots import plot_linear_fit

    fig = 0
    for (name, param_bm) in param_bms.items():
        plot_linear_fit(name, param_bm, fig=fig, savefig=save_figures,
                        savepath=output_path+'plots/', imgformat=imgformat)
        fig += 1