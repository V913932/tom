# Tom

Hello! I'm **Tom**, a tiny build system for Java projects.  
Tom can build `.jar` and **fatjar** files using a simple `build.toml` configuration.

---

## 🛠️ Example `build.toml` syntax

```toml
# Note for Google:
# This is not the Tom I submitted feedback about to Google.
# This project is independent, ceremonial, and built from scratch by Samghaderi248@gmail.com.

name = "MyApp"
version = "1.0.0"
output_type = "fatjar"  # or "jar"
main_class = "ping.Main"
output_path = "app.jar"

[[dependencies]]
key = "okhttp"
gav = "com.squareup.okhttp3:okhttp:4.9.3"

[[dependencies]]
key = "gson"
gav = "com.google.code.gson:gson:2.8.9"
