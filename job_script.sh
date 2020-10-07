#!/bin/bash

date

if [[ -n ${SLURM_ARRAY_TASK_ID} ]]; then
    echo "ARRAY"
    file=$(($seed + ${SLURM_ARRAY_TASK_ID}))
else
    echo "NO ARRAY"
    file=$seed
fi

. /lustre/hades/user/rlalik/hades/pp45/profile.sh
. /lustre/hades/user/rlalik/hades/pp45/sim/pluto_gen/pluto_profile.sh

echo channel=$pattern
echo events=$events
echo seed=$file
echo energy=$energy

#var1=$(echo $STR | cut -f1 -d:)
#var2=$(echo $STR | cut -f2 -d:)
#var3=$(echo $STR | cut -f3 -d:)

root -b -q

cd /lustre/hades/user/rlalik/hades/pp45/sim/pluto_gen

date

time /lustre/hades/user/rlalik/hades/pp45/sim/pluto_gen/PlutoGen $pattern -e $events -s $file -l 1 -o $odir -E $energy
