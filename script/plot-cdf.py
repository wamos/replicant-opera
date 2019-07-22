#!/usr/bin/env python2.7

import re, argparse
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('-l', '--logs', required=True, nargs='+', type=argparse.FileType('r'), help='Log files')
parser.add_argument('--show-legend', required=False, action='store_true', help='Show legend')
#parser.add_argument('-p', '--plot', required=True, choices=[ 'cdf', 'time', 'scatter' ], help='The type of plot')
parser.add_argument('-o', '--output', type=argparse.FileType('w'), help='Output plot to file')
args = parser.parse_args()

if args.output:
    # Workaround w/o X window server
    import matplotlib
    matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.ticker import AutoMinorLocator

class LineState:
    Unknown = 0
    FCT = 1
    TCT = 2

def process_log(f):
    fcts = {}
    tcts = {}
    re_fct_start = re.compile(r'^======== FCT')
    re_tct_start = re.compile(r'^======== TCT')
    state = LineState.Unknown
    for line in f:
        line = line.strip()
        if not line:
            state = LineState.Unknown
            continue
        #end
        if state == LineState.Unknown:
            if re_fct_start.match(line):
                state = LineState.FCT
            elif re_tct_start.match(line):
                state = LineState.TCT
            continue
        elif state == LineState.FCT:
            if line.startswith('Tag'):
                continue
            (tag, fct, count) = line.split(',')
            if tag not in fcts:
                fcts[tag] = {
                    'fct': [],
                    'count': []
                }
            fcts[tag]['fct'].append(float(fct))
            fcts[tag]['count'].append(int(count))
        elif state == LineState.TCT:
            # if line.startswith('Tag'):
            #     continue
            # (tag, fct, count) = line.split(',')
            # if tag not in fcts:
            #     fcts[tag] = {}
            # fcts[tag][float(fct)] = int(count)
            raise ValueError("not implemented")
        else:
            raise ValueError("Unrecognized state when parsing log")
    return (fcts, tcts)

def plot_cdf(d, name):
    x = np.array(d['fct'])
    y = np.cumsum(d['count'], dtype=float)
    y /= y[-1]
    ax = plt.gca()
    ax_color_cycle = ax._get_lines.prop_cycler
    color = ax_color_cycle.next()['color']
    ax.hlines(y=y[:-1], xmin=x[:-1], xmax=x[1:], color=color, label=name)
    y = np.insert(y, 0, 0.)
    ax.vlines(x=x, ymin=y[:-1], ymax=y[1:], color=color)

def set_plot_options():
    ax = plt.gca()
    # ax.xaxis.set_minor_locator(AutoMinorLocator(10))
    ax.yaxis.set_minor_locator(AutoMinorLocator(5))
    ax.grid(which='minor', alpha=0.2)
    ax.grid(which='major', alpha=0.5)

    plt.xlabel('CDF (s)')
    plt.xlim(0.0, None)
    plt.ylim(-0.05, 1.05)
    if args.show_legend:
        plt.legend(bbox_to_anchor=(1.04,1), loc="upper left")
        # plt.tight_layout(rect=[0,0,0.75,1])

def get_label(filename, tag):
    if 'fat_tree' in filename:
        name = 'Fat-Tree'
        m = re.search(r'k=(\d+)', filename)
        if not m:
            raise ValueError("k not detected for Fat-Tree")
        k = int(m.group(1))
        name += ' k=%d' % k
    elif 'rotor' in filename:
        name = "RotorNet (One-hop)"
    return '%s - %s' % (name, tag)

def main():
    plt.figure(figsize=(8, 6))
    for f in args.logs:
        (fcts, tcts) = process_log(f)
        for tag in fcts:
            plot_cdf(fcts[tag], get_label(f.name, tag))

    set_plot_options()
    if args.output:
        plt.savefig(args.output.name, bbox_inches = "tight")
    else:
        plt.show()

if __name__ == "__main__":
    main()
