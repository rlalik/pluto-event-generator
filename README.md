Prepare ssh
===========

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

About GSI and work organization
===============================

A few words about computer system in GSI. There ate three systems: GSI machines, Kronos machines (interface for HPC farm) and the HPC farm itself (you have no direct access there). And there are two storage systems: home and luster. GSI and Kronos have access to home, Kronos and Lustre have access to lustre, GSI has no access to lustre and HPC has no access to home. What more, only GSI has access to the internet. It makes everything complicated, specially if need for accessing internet stuff, like this repo. But we try to make it easy.

Home is a place where you should store your simulation and analysis code (you can access it from GSI and Cronos) but you should install all executables and sim/data on lustre (that kronos and HPC have access there).

We organize work as follow. In your home create `hades/pp35` (or other name which can identify your analysis). Inside create directory `tools` where we download all tools (like this repo).

Most of the work we will do on Kronos (`ssh kro -Y`), however, if action on GSI machine is needed, I will write it or end the command line with #gsi which means, in the ssh conenction to gsi (`ssh gsi -Y`).

Prepare GSI machines
====================

Go to kronos computer `ssh kro -Y`

Create hades work dir

    mkdir ~/hades/pp35
    cd hades/pp35

(for proton+proton 3.5 GeV, for different energies you can create different names).

Make some subdirs for more organization.

    mkdir sim

Go to lustre (see Tips on the end of this file). On lustre make:

    mkdir hades/pp35 -p


Installing Pluto Generator
==========================

Clone this repository in your home in the gsi conenction

    cd hades/pp35/sim/
    git clone https://github.com/hadesuj/pluto_gen pluto_gen
    cd pluto_gen

You must have a profile script. If not yet, you can use an example from this repository (skip this if your profile exists).

    cp profile.sh.example ../../profile.sh

and edit the file woth your favorite editor to adjust `HADDIR` and `HGEANT_DIR` variables.

Source your profile, e.g. `. ../../profile/.sh` and install pluto generator.

    make
    make install


Working with git
================

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



Tips
====

Quick lustre access
-------------------

in your `~/.bash.rc` add these lines to easy access lustre

    function lustre {
        cd /lustre/nyx/hades/user/${USER}
    }

Then is enough to type `lustre` in your cmd to go there, to get back home type `cd`.

You must reload your profile to apply these changes, execute

    . ~/.bash.rc