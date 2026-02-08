#!/usr/bin/env python3
import sys
import re

# 匹配以「// 【学习笔记】」开头的注释行
note_pattern = re.compile(r'^\s*// note.*$', re.MULTILINE)

# 读取输入（Git传入的文件内容）
content = sys.stdin.read()
# 移除匹配的注释行
clean_content = note_pattern.sub('', content)
# 输出处理后的内容（Git会用这个内容暂存）
sys.stdout.write(clean_content)