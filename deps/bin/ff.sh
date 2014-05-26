#!/bin/bash
# 将 canvas_$i_960x540.yuv 转换为 $i.jpg

rm *.jpg -f

i=1

while [ $i -lt 100 ]
do
   filename=canvas_${i}_960x600.yuv
   echo $filename

   if [ -f $filename ]; then
   	./ffmpeg -s 960x600 -pix_fmt yuv420p -i $filename $i.jpg
   else
       break
   fi

   let i++

done

rm *.yuv -f

