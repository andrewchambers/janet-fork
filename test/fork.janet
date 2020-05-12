(use fork)

(if-let [p (fork)]
  (do
    (assert (= 0 (wait p)))
    (assert (= 0 (p :exit-code))))
  (os/sleep 0.1))

(if-let [p (fork)]
  (do
    (:close p)
    (assert (not= 0 (wait p)))
    (assert (not= 0 (p :exit-code))))
  (os/sleep 999))
