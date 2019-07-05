require "rbconfig"

BIN = "./workbench/artsy-in-c"
INCLUDE = [RbConfig::CONFIG["rubyhdrdir"], RbConfig::CONFIG["rubyarchhdrdir"]]
LINK = ["ruby"]

namespace :dev do
  namespace :setup do
    task :vscode do
      require "json"
      settings = JSON.parse(File.read("./.vscode/c_cpp_properties.json.example"))
      include_path = settings["configurations"][0]["includePath"]
      include_path.concat(INCLUDE)
      File.write("./.vscode/c_cpp_properties.json", JSON.pretty_generate(settings))
    end
  end

  task :format do
    sh "clang-format -i *.c"
  end
end

directory "workbench"

task :compile => "workbench" do
  sh "clang #{INCLUDE.map { |i| "-I '#{i}'" }.join(" ")} #{LINK.map { |l| "-l #{l}" }.join(" ")} main.c -o #{BIN}"
end

task :run => :compile do
  sh BIN
end

task :default => :run