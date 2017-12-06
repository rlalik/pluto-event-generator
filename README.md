PlutoGen is a program which generates Pluto events based on the database information.
PlutoGen is not Pluto, it uses Pluto, so don't be confused.

About GSI and work organization
===============================

A few words about computer system in GSI. There are three systems: GSI machines, Kronos machines (interface for HPC farm) and the HPC farm itself (you have no direct access there). And there are two storage systems: home and luster. GSI and Kronos have access to home, Kronos and Lustre have access to lustre, GSI has no access to lustre and HPC has no access to home. Only GSI and Kronos have access to the internet.

Home is a place where you should store your simulation and analysis code (you can access it from GSI and Cronos) but you should install all executables and sim/data on lustre (that kronos and HPC have access there).

Installing PlutoGen Generator
=============================

Go to your lustre directory (see *Tips* section for details). Create sim directory and enter it:
    mkdir hades/pp45/sim -p
    cd hades/pp45/sim/

Clone the repository:

    git clone https://github.com/hadesuj/pluto_gen pluto_gen
    cd pluto_gen

Source your profile (see Tips section if you don't have profile yet), e.g. `. ../../profile/.sh` and install pluto generator.

    make
    make install

Usage
=====

`PlutoGen` uses database to generate output file. To randomize the event, an input `seed` can be passed. It runs `n` loops of `e` events, starting with offset `o`. The final `seed` is calculated for each loop as `seed + offset + i` where `i` is between 0 and `l`. The output file has format: `pluto_chan_#1_seed_#1.*` where `#1` is a channel number and #2 is a final seed number calculated as above.

Database
--------

Program requires a database file, the default name is `ChannelsDatabase.txt`. It contains a list of channels identified by number. Header of the file with a channel for *p+p -> p K+ Lambda* reaction is also listed.

    @ Lines starting with @ are comments
    @ Columns:
    @  Ch            - channel number
    @  Width         - ingore, set to 1.0
    @  Crosssection  - ignore, set to 1.0
    @  Reaction      - channel reaction
    @
    @Ch     Width   Crossection     Reaction
    40      1.0     0.0480451       Lambda,K+,p

Running
-------

Program is executed `Plutogen [options] id` where options are:
    -E  -- energy in GeV (default: 3.5)
    -d  -- database file (default: ChannelsDatabase.txt)
    -f  -- start index of the loop (default: 0)
    -l  -- number of loops (default: 1)
    -s  -- input seed (default: 0)
    -e  -- events (default: 1000)
    -o -- output directory (default: ./)

    id -- a database channel number, only first value is processed

The outpt file is `pluto_chan_#1_seed_#1.*` as described above.

Using Batch Farm
----------------

It is possible to send jobs to the GSI batch farm. For that, two additional files `job_script.sh` and `run_job.py` are provided.

`job_script.sh` is a execution script and usually doesn't need any changes. The same for `run_job.py`.

To send jobs, execute following command:

    ./run_job args [-e events]

where `events` are optional (default: 10000) ang `args` can be multiple entries in format of `id:f:l:s`, see section *Running* for description. All parameters beside (id) are optional but must be passed in sequence: `id`, `id:f`, `id:f:l` or `id:f:l:s`. Examples:

    ./run_job.py -e 100 40:10         # will run 100 events with seed 0+10 9one output file) of channel 40
    ./run_job.py 40:15:100:200        # will run 1000 events with seeds 215..315 (100 output files) of channel 40

If you wish to change the energy, please edit line #12 of `job_script.sh`.

Don't forget to create `log` directory if using the farm.

Tips
====

Prepare ssh
-----------

In your `~/.ssh/config`

    Host                        gsi
        Hostname                lx-pool.gsi.de
        Port                    22
        User                    your_user_name
        ForwardAgent            yes
        ForwardX11Trusted       yes
        TCPKeepAlive            yes
    
    Host                        kro
        User                    your_user_name
        ProxyCommand            ssh -q gsi -W kronos.hpc.gsi.de:%p

Replace `your_user_name` with your GSI user name.

Now you can connect to GSI machine using `ssh gsi -Y` or to farm machines using `ssh kro -Y`. Always use `-Y` parameter i ncase you will need X server.


Prepare environment
--------------------

Go to kronos computer `ssh kro -Y`

Create hades work dir

    mkdir ~/hades/pp45
    cd hades/pp45

(for proton+proton 3.5 GeV, for different energies you can create different names).

Make some subdirs for more organization.

    mkdir sim

Go to lustre (see Tips on the end of this file). On lustre make:

    mkdir hades/pp45 -p

Prepare profile
---------------

You must have a profile script. If not yet, you can use an example from this repository

    cp profile.sh.example ../../profile.sh

and edit the file woth your favorite editor to adjust `HADDIR` (if using custom hydra) and `HGEANT_DIR` variables.

Quick lustre access
-------------------

in your `~/.bash.rc` add these lines to easy access lustre

    function lustre {
        cd /lustre/nyx/hades/user/${USER}
    }

Then is enough to type `lustre` in your cmd to go there, to get back home type `cd`.

You must reload your profile to apply these changes, execute

    . ~/.bash.rc

Working with git
----------------

To clone repository

    git clone address destination

After you made changes, commit them to the repository

    git commit -m "Message text" -a

If you use switch `-a` then all changes are commited, if you wanna commit only selected files, put file names instead of `-a`.

    git push

will push the data to remote repository.

If you want to fetch recent changes from the repository

    git pull

All these actions you must do inside the repository directory: the `destination` parameter of the clone directory.
