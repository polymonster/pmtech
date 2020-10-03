mkdir thumbs
find . -iname "*jpg" -exec ffmpeg -i {} -vf scale="640x360" thumbs/{} \;