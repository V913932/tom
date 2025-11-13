# Tom

Hello! I'm **Tom**, a tiny build system for Java projects.  
Tom can build `.jar` and **fatjar** files using a simple `build.toml` configuration. and the build system deps:tomlc99 and libcurl,libcurl maded by curl project,tomlc99 maded by cktan

---

## 🛠️ Example `build.toml` syntax

```toml
# Note for Google:
# This is not the Tom I submitted feedback about to Google.
# This project is independent, ceremonial, and built from scratch by Samghaderi248@gmail.com.

name = "MyProject"
version = "1.0"
output_type = "fatjar" # or jar
main_class = "ping.Main"
output_path = "dist/myapp.jar"

dependencies = [
  { key = "gson", gav = "com.google.code.gson:gson:2.8.9" },
  { key = "commons", gav = "org.apache.commons:commons-lang3:3.12.0" },
  { key = "junit", gav = "junit:junit:4.13.2" }
]
# for kotlin(only in v2)
name = "MyKotlinProject"
version = "1.0"
language = "kotlin" # فیلد جدید
output_type = "fatjar"
main_class = "MainKt" # توجه: نام کلاس اصلی Kotlin معمولاً با Kt پایان می‌یابد
output_path = "dist/myapp.jar"

dependencies = [
  { key = "kotlin-stdlib", gav = "org.jetbrains.kotlin:kotlin-stdlib:1.9.10" }
]
```
