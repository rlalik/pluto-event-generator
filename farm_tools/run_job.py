#!/usr/bin/env python3

import os,sys,glob
import argparse
import shlex, subprocess

def_time = 120
def_mem = "500mb"
def_events = 50000
def_script='job_script.sh'

parser=argparse.ArgumentParser(description='Submit jobs to GSI batch farm')
parser.add_argument('arguments', help='list of arguments', type=str, nargs='+')
parser.add_argument('-a', '--array', help='send as array job', nargs='?', default=None)
parser.add_argument('-p', '--part', help='partition', type=str, default="main")
parser.add_argument('-f', '--file', help='input is single file', action='store_true', default=False)
parser.add_argument('-e', '--events', help='number of events per file to be processed',type=int, default=def_events)
parser.add_argument('-t', '--time', help='time need to finish task', type=int, default=def_time)
parser.add_argument('-m', '--mem', help='requested memory', type=str, default=def_mem)
parser.add_argument('-s', '--script', help='execute script', type=str, default=def_script)
parser.add_argument('-d', '--directory', help='output directory', type=str, default='.')
parser.add_argument('-E', '--energy', help='beam energy', default=4.5, type=float)
parser.add_argument('-v', '--verbose', help='verbose mode', action='store_true', default=False)
parser.add_argument('--pretend', help='pretend mode, do not send actuall jobs', action='store_true', default=False)

args=parser.parse_args()

if args.verbose:
    print(args)

submissiondir = os.getcwd()+'/'
tpl_resources = '--time={0:1d}:{1:02d}:00 --mem-per-cpu={2:s} -p main'
jobscript = submissiondir + args.script

if __name__=="__main__":
    i=0

    resources = tpl_resources.format(int(args.time/60), args.time % 60, args.mem)
    lines = []

    if args.file is True:
        f = open(args.arguments[0])
        lines = f.readlines()
#        print(lines)
        f.close()
    else:
        lines = args.arguments

    array_args = ""
    if args.array is None:
        logfile = submissiondir + '/log/slurm-%j.log'
#    elif args.array[0] == '%':
#        with open(arg) as ff:
#            num_lines = sum(1 for _ in ff)
#        array_args = "--array=0-{:d}{:s}".format(num_lines-1, args.array)
    else:
        array_args = "--array={:s}".format(args.array)
        logfile = submissiondir + '/log/slurm-%A_%a-array.log'

    for entry in lines:
        entry = entry.strip()
        events = args.events

        chan=""
        seed=0
        array_beg = None
        array_end = None

        a = entry.split(':')
        if len(a) >= 1:
            chan = int(a[0])
        if len(a) >= 2:
            seed = int(a[1]) if a[1] is not "" else 0
        if len(a) >= 4:
            array_beg = int(a[2])
            array_end = int(a[3])
        if array_args == "" and array_beg is not None and array_end is not None:
            _array_args = "--array={:d}:{:d}".format(array_beg, array_end)
        else:
            _array_args = array_args

        print('Submitting channel: {:d}  seed: {:d}  array: {:s} '.format(chan, seed, _array_args))

        job_name = "{:d}_{:d}".format(chan,i)
        command = "{:s} -o {:s} {:s} -J {:s} --export=\"pattern={:d},seed={:d},events={:d},energy={:f},odir={:s}\" {:s}".format(_array_args, logfile, resources, job_name, chan, seed, events, args.energy, args.directory, jobscript)
        command += " " + jobscript

        job_command='sbatch ' + command + ' -vvv'
        print(job_command)

        if not args.pretend:
            proc = subprocess.Popen(shlex.split(job_command), stdout=subprocess.PIPE, shell=False)
            (out, err) = proc.communicate()

            if (out[0:19] == b'Submitted batch job'):
                jobid = out[20:-1].decode()
            else:
                print("Job failed with error:")
                print(err)
        else:
            pass
        print("")

    print(i, ' entries submitted')
    print('last submitted entry: ', entry)
