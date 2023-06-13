PlutoGen is a simple program which generates Pluto events based on the command-line or database information.
PlutoGen is not Pluto, it uses Pluto, so don't be confused.

It allows to prepare a database of final state particles, define the reaction system and energy.

Usage Examples
============
Generate events from channel 1 from the database (what is database see below).
```bash
./PlutoGen 1
```
Generate events from channel 1 from the database for the p+p @ 4.5 GeV beam kinetic energy (fixed target).
```bash
./PlutoGen 1 -c p,p,4.5
```
Generate events for `p n K+ K0S` final state particles in the p+p @ 4.5 GeV collision (fixed target) system
```bash
./PlutoGen p,n,K+,K0S -c p,p,4.5
```
Change the beam energy of the system defined in the database
```bash
./PlutoGen 1 -T 5.5
```

Installing PlutoGen Generator
=============================

Instalaltion in based on CMake build system, and follow standard cmake style.
```bash
mkdir build
cd build
cmake ..
```
Use `-DPluto_DIR=PATH` if cmake cannot find your pluto installation.

Build Requirements
------------------------
* Pluto package - https://github.com/PlutoUser/pluto6
* Cern's ROOT package (also dependency of Pluto) - https://github.com/root-project/root

Usage
=====
`PlutoGen` uses database to generate output file. To randomize the event, an input `seed` can be passed. It runs `n` loops of `e` events for seeds from `seed` to `seed+n`. See `PlutoGen -h` for details and examples above.

Input arguments
-------------------
The input can be database channel or body string, e.g. If giving the reaction body, the collision system must be specified.
```bash
./PlutoGen 20                          # channel 20
./PlutoGen p,p,pi0 -c p,p,2.0   # reaction body with collision system
```
The output file has format: `pluto_chan_#1_events_#2_seed_#3.{evt,root}` where:
* `#1` is a databse channel or the command line reaction body
* `#2` is a number of events
* `#3` is a seed number.

For the examle above the oputput files would be:
```
pluto_chan_020_events_10000_seed_0000.evt
pluto_chan_p,p,pi0_events_10000_seed_0000.evt
```
Database
=======

Program can use a database file, the default expected name is `ChannelsDatabase.txt`.
It contains a list of channels identified by number. Database may contain the collision system sepcification (if not, then must be speicfied from command line option `-c`, `--collision`).
Lines starting with `#` are comments, the comments can be put after the channel specification, otherwise the entry is ill-formed.

Database header example with a few channels:
```
# Lines starting with # are comments
# Columns:
#  Ch           - channel number
#  Width        - ingore, set to 0.0
#  Crosssection - ignore, set to 0.0
#  Reaction     - channel reaction
#
#  Ch Width  Crossection   Reaction
#
# The config file must specify the collision system.
# Start this with @ and follow with pattern: beam,target,energy_in_GeV
@ p,p,4.5
# pp elastic and quasi-elastics
#   0   0.0     0.0     p,p                   # elastic scateering must be specified differenty, currently not working
    1   0.0     0.0     p,p,pi0
    2   0.0     0.0     p,p,pi0,pi0
    3   0.0     0.0     p,p,pi+,pi-

   20   0.0     0.0     p,n,pi+
   21   0.0     0.0     p,n,pi+,pi0
```

The reaction must be a single string (no white spaces allowed) of the particles know to Pluto.

The `width` and `cross-section` paramaters may be supported in the future, thus are left in the entry format.
