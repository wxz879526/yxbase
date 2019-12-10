
echo "拷贝base"
copy .\base\ .\pack\base\ -Recurse -Verbose -Force

echo "拷贝build"
copy .\build\ .\pack\build\ -Recurse -Verbose -Force

echo "拷贝gen"
copy .\out\gen\ .\pack\out\gen\ -Recurse -Verbose -Force

echo "拷贝testing"
copy .\testing\ .\pack\testing\ -Recurse -Verbose -Force

echo "拷贝third_party"
copy .\third_party\googletest\ .\pack\third_party\googletest\ -Recurse -Verbose -Force

echo "拷贝lib"
md .\pack\lib
copy .\libcore.lib .\pack\lib\ -Verbose -Force

Pause