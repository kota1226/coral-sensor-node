#!/bin/bash

echo cd $(dirname $(realpath -e $0))/..
cd $(dirname $(realpath -e $0))/..

pwd
echo

for f in TFT_Drivers/HX8357D_Init.h TFT_Drivers/HX8357D_Rotation.h User_Setup.h User_Setups/Setup15_HX8357D.h 
do
  echo $f '*************************************'
  echo diff port-lvgl/lib9/TFT_eSPI_files/$(basename $f) lib9/TFT_eSPI/$f
done

echo
echo lv_conf.h '*************************************'
diff port-lvgl/lib9/lv_conf.h lib9/lvgl/lv_conf_template.h

echo
echo LVGL git status '*************************************'
(cd lib9/lvgl; git status)

echo
echo TFT_eSPI git status '*************************************'
(cd lib9/TFT_eSPI; git status)
