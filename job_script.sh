#!/bin/bash

date

. /lustre/nyx/hades/user/rlalik/hades/pp35/profile.sh

echo channel=$pattern
echo offset=$offset
echo loops=$loops
echo events=$events
echo seed=$seed

var1=$(echo $STR | cut -f1 -d:)
var2=$(echo $STR | cut -f2 -d:)
var3=$(echo $STR | cut -f3 -d:)

root -b -q

cd /lustre/nyx/hades/user/rlalik/hades/pp35/sim/pluto_gen

date

time /lustre/nyx/hades/user/rlalik/hades/pp35/sim/pluto_gen/PlutoGen $pattern -e $events -f $offset -l $loops -s $seed -o output
