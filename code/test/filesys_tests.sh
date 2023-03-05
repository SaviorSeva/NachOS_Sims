#!/bin/bash
#../build/nachos-step5 -f -cp ../filesys/test/small smol_file -mkdir dir_1 -cd dir_1 -mkdir dir_2 -cp ../filesys/test/medium med_file -cd .. -cp ../filesys/test/big big_file -l -cd dir_1 -l -cd dir_2 -l -quit
#ARG = t t1 t2 openfile multifile multithreadwrite
case $1 in

    t)
        ../build/nachos-final -quit -f -t
        ;;
    t1)
        ../build/nachos-final -quit -f -t1
        ;;
    t2)
        ../build/nachos-final -quit -f -t2
        ;;
    openfile)
        ../build/nachos-final -quit -f -cp ../filesys/test/big big -cp ../build/openfiletest of -x of -D
        ;;
    multifile)
        ../build/nachos-final -quit -f -cp ../filesys/test/big big -cp ../filesys/test/medium med -cp ../build/multifiletest mf -x mf -D
        ;;
    multithreadwrite)
        ../build/nachos-final -quit -f -cp ../filesys/test/big big -cp ../build/multiThreadWrite mtw -x mtw -D
        ;;
    *)
        echo "Unknown argument value, try again"
        echo "accepted values: t, t1, t2, openfile, multifile, multithreadwrite"
        ;;
esac

