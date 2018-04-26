#!/usr/bin/env python

#----------------------- AUTHOR: J. Berger-Chen, July 2014 -------------------------------
#CALL EXAMPLE:
#python run_dst.py /hera/hades/dst/jul14/online/187/root/ test/ la_187_test 100 La --nFiles=1
#python run_dst.py /hera/hades/dst/jul14/online/188/root/ k0_188/ k0_188 K0 10000000
#python run_dst.py /hera/hades/dst/jul14/online/187/root/ la_187/ la_187 La 10000000

# modified by R. Lalik

import os,sys,glob
#sys.path.append('/hera/hades/user/klapidus/online_rec/python/argparse-1.2.1')
import argparse

def_time = 120
def_mem = "500mb"
def_script='job_script.sh'
def_dir='./'

parser=argparse.ArgumentParser(description='Submit dst analysis to GSI batch farm')
parser.add_argument('arguments', help='list of arguments', type=str, nargs='+')
parser.add_argument('-p', '--part', help='partition', type=str, default="main")
parser.add_argument('-f', '--file', help='input is in a file', action='store_true', default=False)
parser.add_argument('-e', '--events', help='number of events per file to be processed',type=int, default=25000)
parser.add_argument('-t', '--time', help='time need to finish task', default=def_time, type=int)
parser.add_argument('-m', '--mem', help='requested memory', default=def_mem, type=str)
parser.add_argument('-s', '--script', help='execute script', default=def_script)
parser.add_argument('-d', '--dir', help='working directory', default=def_dir)
parser.add_argument('-E', '--energy', help='beam energy', default=4.5, type=float)
args=parser.parse_args()

print args

submissiondir=os.getcwd()+'/'
tpl_resources='--time={0:1d}:{1:02d}:00 --mem-per-cpu={2:s} -p main'
jobscript=submissiondir+'job_script.sh'

if __name__=="__main__":
    i=0

    resources = tpl_resources.format(int(args.time/60), args.time % 60, args.mem)
    lines = []

    if args.file is True:
        f = open(args.arguments[0])
        lines = f.readlines()
        print(lines)
        f.close()
    else:
        lines = args.arguments

    for entry in lines:
        entry = entry.strip()
        print entry

        logfile = submissiondir + '/log/slurm-%j.log'
        events = args.events

        print 'submitting file: ',entry

        if os.path.isfile(logfile):
            os.remove(logfile)

        chan=""
        offset=0
        loops=1
        seed=0

        a = entry.split(':')
        if len(a) >= 1:
            chan = a[0]
        if len(a) >= 2:
            offset = int(a[1])
        if len(a) >= 3:
            loops = int(a[2])
        if len(a) >= 4:
            seed = int(a[3])

        for i in xrange(offset, offset+loops):
            job_name = "{:s}_{:d}".format(chan,i)
            command = "-o {:s} {:s} -J {:s} --export=\"pattern={:s},offset={:d},loops={:d},seed={:d},events={:d},energy={:f}\" {:s}".format(logfile, resources, job_name, chan, i, 1, seed, events, args.energy, jobscript)
            qsub_command='sbatch ' + command
            print qsub_command
            os.system(qsub_command)

    print i, ' entries submitted'
    print 'last submitted entry: ',entry
