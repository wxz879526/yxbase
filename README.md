### libcore库介绍
* Chromium Base库是一套十分强大的且跨平台的开发库，提供了（内存/消息循环/线程/文件/字符串/文字编码/json/时间/定时器……）等开发中经常会使用到的功能实现，由Google完成开发，性能及稳定性值得信任。
* libcore基于Chromium 60.0.3112.90版本抽离base库
* libcore使用Chromium的GN + Ninja方式进行构建和编译（目前支持Win/Mac平台，暂未生成其他系统的GN工具到buildtools文件夹）

### libcore库编译
* 安装depot_tools(包含GN和Ninja工具)，修改compile.bat里面的depot_tools 路径
* 添加环境变量 GYP_MSVS_OVERRIDE_PATH  指向vs2017的安装路径 C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise
* 添加环境变量 WINDOWSSDKDIR 指向本地SDK路径 C:\Program Files (x86)\Windows Kits\10
* 启动vs2017的command prompt(x86 Native Tools Command Prompt for VS 2017 32位的)命令行工具 运行compile.bat
* 使用args.gn文件内的编译配置
* 默认输出在out目录
* vs命令行运行create_libcore_run_in_vs_cmd.bat脚本合并最终的libcore.lib
* 在Win10 + VS2017 编译/验证/测试通过

### libcore库使用
**1. 在GN工程使用**
```
 executable("example") {
 output_name = "example"

 deps = [
  "//base:base",
 ]
 
 sources = [
  "example/example.cc",
 ]
}
 
```

**2. 在VS工程使用**

* 包含目录：libcore path
* 链接库文件：

```
yxbase.lib
advapi32.lib
comdlg32.lib
dbghelp.lib
delayimp.lib
dnsapi.lib
gdi32.lib
kernel32.lib
msimg32.lib
odbc32.lib
odbccp32.lib
ole32.lib
oleaut32.lib
psapi.lib
shell32.lib
shlwapi.lib
user32.lib
usp10.lib
uuid.lib
version.lib
wininet.lib
winmm.lib
winspool.lib
ws2_32.lib
```

### 一个调用例子
```
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/files/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include <iostream>

int main() {
  base::FilePath dir;
  PathService::Get(base::DIR_EXE, &dir);

  std::cout << "compute " << base::UTF16ToUTF8(dir.value()) << " size..." << std::endl;
    int64_t dir_size = base::ComputeDirectorySize(dir);

  std::cout << base::IntToString(dir_size) << std::endl;
  
  system("pause");
}
```
