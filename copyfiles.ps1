
echo "拷贝base"
copy .\base\ .\pack\base\ -Recurse -Verbose -Force

echo "拷贝gen"
copy .\out\gen\ .\pack\out\gen\ -Recurse -Verbose -Force

echo "拷贝lib"
md .\pack\lib
copy .\libcore.lib .\pack\lib\ -Verbose -Force

Pause