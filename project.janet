(declare-project
  :name "fork"
  :author "Andrew Chambers"
  :license "MIT"
  :url "https://github.com/andrewchambers/janet-fork"
  :repo "git+https://github.com/andrewchambers/janet-fork.git")

(declare-native
  :name "_jmod_fork"
  :source ["fork.c"])

(declare-source
  :source ["fork.janet"])

