(import _jmod_fork :as _fork)

# kill -l
# It seems these numbers are standard enough, we 
# define them instead of C so tree shaking can remove them in envs.
(def SIGHUP 1)
(def SIGINT 2)
(def SIGQUIT 3)
(def SIGKILL 9)
(def SIGUSR1 10)
(def SIGUSR2 12)
(def SIGPIPE 13)
(def SIGTERM 15)

(defn fork
  ``
    Fork the current process, returning nil in the child,
    or a process object in the parent.

    N.B. Extreme care must be taken when using fork. There is no way to prevent
    the janet garbage collector from running object destructors
    in both vm's after the fork. If active destructors are not safe to
    run twice, it make cause unexpected behavior. An example of this
    would be corrupting an sqlite3 database, as it is open in two processes
    after the fork.

    Accepts the following kwargs:

    :close-signal Signal to send to child on close or gc.
  ``
  [&keys {:close-signal close-signal}]
  
  (_fork/fork close-signal))

(defn wait
  "Wait for the process to exist and return the exit status."
  [p]
  (_fork/wait p))


(defn close 
  "Send the process it's close signal and wait for it to exit."
  [p]
  (_fork/close p))

