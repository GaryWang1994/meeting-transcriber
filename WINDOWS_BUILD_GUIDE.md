# Meeting Transcriber - Windows 版本编译指南

## 方案1：使用预编译的 Linux 版本（推荐）

项目已成功编译 Linux 版本：
```
/home/garywang/Desktop/project/meeting-transcriber/build/bin/meeting-transcriber
```

**在 Windows 上使用：**
1. 安装 WSL (Windows Subsystem for Linux)
2. 在 WSL 中运行 Linux 版本

---

## 方案2：GitHub Actions 自动构建（最简单）

项目已配置 `.github/workflows/build-windows.yml`

**操作步骤：**

### 1. 在 GitHub 创建仓库
- 访问 https://github.com/new
- 创建名为 `meeting-transcriber` 的仓库

### 2. 推送代码到 GitHub
```bash
cd /home/garywang/Desktop/project/meeting-transcriber
git remote add github https://github.com/你的用户名/meeting-transcriber.git
git push -u github master
```

### 3. 下载 Windows 版本
- 推送后，GitHub Actions 自动开始构建
- 进入仓库 → Actions 页面查看进度
- 构建完成后，下载 Artifacts 中的 Windows 版本

---

## 方案3：在 Windows 电脑上本地编译

如果你有 Windows 电脑，这是最可靠的方法：

### 步骤1：安装 MSYS2
1. 下载：https://www.msys2.org/
2. 运行安装程序，默认安装到 `C:\msys64`
3. 安装完成后，打开 "MSYS2 MinGW 64-bit" 终端

### 步骤2：安装编译工具
在 MSYS2 终端中运行：
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make git
```

### 步骤3：克隆并编译项目
```bash
# 克隆仓库（从 Gitea）
git clone http://192.168.1.252:3000/gary/meeting-transcriber.git
cd meeting-transcriber

# 创建构建目录
mkdir build && cd build

# 配置 CMake
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# 编译
mingw32-make -j4
```

### 步骤4：找到可执行文件
编译完成后，`meeting-transcriber.exe` 位于：
```
meeting-transcriber\build\bin\meeting-transcriber.exe
```

### 步骤5：打包分发
将以下文件打包为 ZIP：
```
meeting-transcriber.exe
README.txt (使用说明)
*.dll (依赖库，如果有)
```

---

## 编译注意事项

### 常见问题

1. **编译错误：找不到头文件**
   - 确保 MSYS2 完全安装
   - 运行 `pacman -Syu` 更新所有包

2. **链接错误：找不到库**
   - 检查 CMake 配置输出
   - 确保所有依赖库已安装

3. **运行时缺少 DLL**
   - 将所有依赖的 DLL 复制到可执行文件同一目录
   - 使用工具如 `Dependencies` 检查依赖关系

### 性能优化

编译时添加优化选项：
```bash
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -march=native"
```

---

## 联系我们

- Gitea: http://192.168.1.252:3000/gary/meeting-transcriber
- 项目文档: 参见 README.md

---

**推荐方案总结：**
- **最快获取 Windows 版本**: 使用 GitHub Actions（方案2）
- **最可靠**: 在 Windows 本地编译（方案3）
- **最简单**: 使用 Linux 版本 + WSL（方案1）
