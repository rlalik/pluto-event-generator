PlutoGen is a program which generates Pluto events based on the database information.
PlutoGen is not Pluto, it uses Pluto, so don't be confused.

Installing PlutoGen Generator
=============================

Go to your lustre directory (see *manuals:Tips* repo for details). Create sim directory and enter it:
    mkdir hades/pp45/sim -p
    cd hades/pp45/sim/

Clone the repository:

    git clone https://github.com/HADES-Cracovia/pluto_gen pluto_gen
    cd pluto_gen

Source your profile (see *manuals:Tips* repo if you don't have profile yet), e.g. `. ../../profile/.sh` and install pluto generator.

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

Using with a Batch Farm
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


NOTES
=====

Branching ratios for selected channels:
---------------------------------------

**48 - L(1520) -> L dalitz**

Total cross-section needs to be scaled for BR = 0.0085/137 = 6.2e-05

**49 - L(1520) -> anything than Dalitz**

Five contributions

| Decay channel      | BR       |
|--------------------|----------|
| Simga0 + pi0       | 0.139096 |
| Sigma0 + pi0 + pi0 | 0.009/5  |
| Sigma0 + pi+ + pi- | 0.009/5  |
| Lambda + pi0 + pi0 | 0.1/3    |
| Lambda + gamma     | 0.0085   |
|--------------------|----------|
| TOTAL              | 0.184529 |

**50 - L(1405) -> L dalitz**

Total cross-section needs to be scaled for BR = 0.0085/137 = 6.2e-05

**51 - L(1405) -> anything than Dalitz**

Five contributions

| Decay channel      | BR       |
|--------------------|----------|
| Simga0 + pi0       | 0.33     |
| Sigma+ + pi-       | 0.33     |
| Sigma- + pi+       | 0.3      |
| Lambda + gamma     | 0.0085   |
|--------------------|----------|
| TOTAL              | 0.9985   |

**52 - S(1305) -> L dalitz**

Total cross-section needs to be scaled for BR = 0.0125/137 = 9.124e-05

**53 - S(1305) -> anything than Dalitz**

Five contributions

| Decay channel      | BR       |
|--------------------|----------|
| Simga0 + pi0       | 0.12/3   |
| Lambda + pi0       | 0.87     |
| Lambda + gamma     | 0.0125   |
|--------------------|----------|
| TOTAL              | 0.9225   |