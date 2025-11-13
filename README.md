# tom
Hello,it's tom,tom is a tiny build system that can make jar and fatjar,it's a syntax of the build.toml:```tomlname = "MyApp"
version = "1.0.0"
output_type = "fatjar"  # or "jar"
main_class = "ping.Main"
output_path = "app.jar"

[[dependencies]]
key = "okhttp"
gav = "com.squareup.okhttp3:okhttp:4.9.3"

[[dependencies]]
key = "gson"
gav = "com.google.code.gson:gson:2.8.9"``` note for google:this is not the tom I feedback to the Google
