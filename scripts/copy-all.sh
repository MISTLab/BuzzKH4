#!/bin/bash

# Usage ./copy-all.sh destination_path name (exec|src|all) K01 K02 ... K_n
if [ "$#" -lt 4 ]
then
  echo "Usage: ./copy-all.sh destination_path name (exec|src|all) K01 K02 ... K_n"
  exit 1
fi

DEST_PATH=$1
shift
NAME=$1
shift
OPTION=$1
shift

for khepera in "$@"
do
  ssh $khepera "mkdir -p $DEST_PATH"
  if [ $OPTION == "src" ]
  then
    scp -r $NAME.bzz $khepera:$DEST_PATH
  elif [ $OPTION == "exec" ]
  then
    scp -r $NAME.bo $khepera:$DEST_PATH
    scp -r $NAME.bdb $khepera:$DEST_PATH
  elif [ $OPTION == "all" ]
  then
    scp -r $NAME.bo $khepera:$DEST_PATH
    scp -r $NAME.bdb $khepera:$DEST_PATH
    scp -r $NAME.bzz $khepera:$DEST_PATH
  fi
done
