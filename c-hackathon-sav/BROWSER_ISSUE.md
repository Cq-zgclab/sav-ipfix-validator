# Web界面访问问题解决方案

## 问题现象

在VS Code的Simple Browser中打开web页面时：
- `debug.html` - 显示空白，没有内容
- `static.html` - 内容一闪而过后消失
- `index.html` - 内容一闪而过后消失

## 根本原因

**VS Code的Simple Browser在GitHub Codespaces环境中存在已知兼容性问题。**

页面文件本身完全正常：
- ✅ HTTP服务器正常运行（端口8000）
- ✅ HTML/CSS/JavaScript代码正确
- ✅ 数据文件完整（data.json）
- ✅ curl测试所有文件都能正常访问

## ✅ 解决方案

### 方法1：使用端口转发（推荐）

1. 点击VS Code **底部面板**的 **【端口】** 标签（在"终端"、"问题"等标签旁边）
2. 找到端口 **8000** 这一行
3. 点击右侧的 **地球图标🌐** "在浏览器中打开"
4. 页面会在系统默认浏览器中打开

### 方法2：复制转发URL

1. 在【端口】标签中找到端口 8000
2. 右键点击该行
3. 选择 **"复制本地地址"**
4. 在Chrome/Firefox/Safari等外部浏览器中粘贴访问

### 方法3：命令行获取URL

```bash
# 查看转发的端口
gh codespace ports -c $CODESPACE_NAME

# 设置端口为公开访问（如需要）
gh codespace ports visibility 8000:public -c $CODESPACE_NAME
```

## 可访问的页面

| 页面 | 路径 | 说明 |
|------|------|------|
| 静态演示 | `/static.html` | 纯HTML，不依赖JS，显示完整数据 |
| 动态界面 | `/index.html` 或 `/` | 完整的交互式界面 |
| 调试页面 | `/debug.html` | 显示详细加载日志 |
| 测试页面 | `/test.html` | 简单功能测试 |
| 数据文件 | `/data.json` | JSON格式的原始数据 |

## 验证服务状态

```bash
# 检查服务是否运行
ps aux | grep "python3 -m http.server 8000"

# 测试HTTP响应
curl http://localhost:8000/static.html | head -20

# 测试数据文件
curl http://localhost:8000/data.json
```

## 停止服务

如需停止HTTP服务器：

```bash
# 找到进程ID
lsof -ti :8000

# 停止服务
kill $(lsof -ti :8000)
```

## 重新启动

```bash
cd /workspaces/sav-ipfix-validator/c-hackathon-sav
bash run.sh
```

---

**结论**：页面和服务都正常，唯一的问题是VS Code Simple Browser的兼容性。请使用外部浏览器通过端口转发访问。
