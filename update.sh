#!/bin/bash

echo -e "\nupdate script for CubeSatSim\n"

if [ "$1" = "u" ]; then

  sudo apt-get update && sudo apt-get dist-upgrade -y

  sudo apt-get install -y wiringpi git libasound2-dev i2c-tools cpulimit

fi

cd /home/pi/CubeSatSim

git pull > .updated

make debug

FLAG=0

if [[ $(diff systemd/cubesatsim.service /etc/systemd/system/cubesatsim.service) ]]; then
  echo "changed cubesatsim.service"
  sudo cp /home/pi/CubeSatSim/systemd/cubesatsim.service /etc/systemd/system/cubesatsim.service
  FLAG=1
else
  echo "no changes to cubesatsim.service"
fi

if [[ $(diff systemd/rpitx.service /etc/systemd/system/rpitx.service) ]]; then
  echo "changed rpitx.service"
  sudo cp /home/pi/CubeSatSim/systemd/rpitx.service /etc/systemd/system/rpitx.service
  FLAG=1
else
  echo "no changes to rpitx.service"
fi

FILE=/home/pi/CubeSatSim/sstv_image_1_320_x_256.jpg
if [ ! -f "$FILE" ]; then
    echo "Copying SSTV image 1"
    cp /home/pi/CubeSatSim/sstv/sstv_image_1_320_x_256.jpg /home/pi/CubeSatSim/sstv_image_1_320_x_256.jpg
fi
    
FILE=/home/pi/CubeSatSim/sstv_image_2_320_x_256.jpg
if [ ! -f "$FILE" ]; then
    echo "Copying SSTV image 2"
    cp /home/pi/CubeSatSim/sstv/sstv_image_2_320_x_256.jpg /home/pi/CubeSatSim/sstv_image_2_320_x_256.jpg
fi
      
grep 'update' /home/pi/CubeSatSim/.updated
if [[ $(grep 'update' /home/pi/CubeSatSim/.updated) ]]; then
  echo "update script updated"
  /home/pi/CubeSatSim/update
fi

if [ $FLAG -eq 1 ]; then
  echo "systemctl daemon-reload and restart"
  sudo systemctl daemon-reload 
  sudo systemctl restart cubesatsim
else
  grep 'changed' /home/pi/CubeSatSim/.updated
  if [[ $(grep 'changed' /home/pi/CubeSatSim/.updated) ]]; then
    echo "systemctl restart cubesatsim"
    sudo systemctl restart cubesatsim
  else
    echo "nothing to do"
  fi  
fi
