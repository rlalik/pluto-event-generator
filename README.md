
PlutoGen is a simple program which generates Pluto events based on the command-line or database information.
PlutoGen is not Pluto, it uses Pluto, so don't be confused.

It allows to prepare a database of final state particles, define the reaction system and energy.

# Usage Examples

Generate events from channel 1 from the database (what is database see below).
```bash
PlutoGen 1
```
Generate events from channel 1 from the database for the p+p @ 4.5 GeV beam kinetic energy (fixed target).
```bash
PlutoGen 1 -c p,p,4.5
```
Generate events for `p n K+ K0S` final state particles in the p+p @ 4.5 GeV collision (fixed target) system
```bash
PlutoGen p,n,K+,K0S -c p,p,4.5
```
Change the beam energy of the system defined in the database
```bash
PlutoGen 1 -T 5.5
```
List all database reactions (query)
```bash
PlutoGen --query
```
List the selected database reactions with additional beam energy change:
```bash
PlutoGen --query 1 -T 5.5
```
# Installing PlutoGen Generator

Installation in based on CMake build system, and follow standard CMake style.
```bash
mkdir build
cd build
cmake ..
```
Use `-DPluto_DIR=PATH` if CMake cannot find your Pluto installation.

## Build Requirements

* Pluto package - https://github.com/PlutoUser/pluto6
* Cern's ROOT package (also dependency of Pluto) - https://github.com/root-project/root

# Usage

`PlutoGen` uses database to generate output file. To randomise the event, an input `seed` can be passed. It runs `n` loops of `e` events for seeds from `seed` to `seed+n`. See `PlutoGen -h` for details and examples above.

## As a event generator

The input can be database channel or body string, e.g. If giving the reaction body, the collision system must be specified.
```bash
./PlutoGen 20                   # channel 20
./PlutoGen p,p,pi0 -c p,p,2.0   # reaction body with collision system
```
The output file has format: `pluto_chan_#1_events_#2_seed_#3.{evt,root}` where:
* `#1` is a database channel or the command line reaction body
* `#2` is a number of events
* `#3` is a seed number.

For the example above the output files would be:
```
pluto_chan_020_events_10000_seed_0000.evt
pluto_chan_p,p,pi0_events_10000_seed_0000.evt
```

## As a database browser
There is an option `--query` which will list the database entry instead of running the event generator. If a channel numbers are given only selected channels will be query. An example output of the query mode looks like:
```text
Channel 661 : sqrt(s) = 3.459172 (p + p @ 4.5 GeV) M = 3.430564   Lambda + Sigma0 + K+ + K+ + pi0
Channel 662 : sqrt(s) = 3.459172 (p + p @ 4.5 GeV) M = 3.565541 ! Lambda + Sigma0 + K+ + K+ + pi0 + pi0
```
and contains:
* channel id
* `sqsrt(s)` is to total energy in [GeV] of given reaction (in the brackets)
* `M = `  is the channel threshold; if it is followed by `!` then the channel has threshold above available energy
* the channel decay string

The available energy calculation will be changed with change of the collision system (`-c` option) or the beam energy (`-T` option).

# Database

Program can use a database file, the default expected name is `ChannelsDatabase.txt`.
It contains a list of channels identified by number. Database may contain the collision system specification (if not, then must be specified from command line option `-c`, `--collision`).
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

The `width` and `cross-section` parameters may be supported in the future, thus are left in the entry format.

## Subsequent decays

If you want to have specific decays chain, use decay modifier `[DECAYMODE]` to the particle, where `DECAYMODE` is the requested subsequent decay. For example to simulate radiative decay of `L1520` you will type:
```text
p,K+,L1520[S1385,g]
```
The decays can be chained, e.g., to further decay `S1385` and `L(1116)` (you rather want Geant to decay `L(1116)` as this is displaced vertex, but this is just an example), you will type:
```text
p,K+,L1520[S1385[Lambda[p,pi-],pi+,pi-],g]
```

Sometime there might be request to decay the particle into all possible decay modes, e.g. one wants to have a `p+p -> p,p,pi0` reaction but also want `pi0` to be directly decayed. It can be achieved by adding `[decay]` modifier to the particle. The entry should look: `p,p,p0[decay]`. The output will be a cocktail of all `pi0` decay modes.

Additionally, if the decay modes contains other particles which should be also decayed, one can add subsequent decay request. For that type requested decays add each requested particle guarded with `:`.  Multiple particles can be listed here.

For example, one wants to simulate a decay of `L(1520)` and if it contains `S(1385)` or `L(1116)`  then also decay them. Then it would be written as `p,K+,L1520[decay:S1385:Lambda:]`

The `[decay]` cannot be followed by another `[]` (it is not supported and already you requested all possible decays, so you cannot request anything more specific).
