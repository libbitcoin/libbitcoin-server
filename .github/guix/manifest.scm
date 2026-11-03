;; Copyright (c) 2014-2026 libbitcoin-server developers (see COPYING).

(use-modules ((gnu packages base) #:select (coreutils-minimal findutils gnu-make grep sed tar which))
             ((gnu packages bash) #:select (bash-minimal))
             ((gnu packages cmake) #:select (cmake-minimal))
             ((gnu packages compression) #:select (gzip))
             ((gnu packages gawk) #:select (gawk))
             ((gnu packages version-control) #:select (git-minimal))
             (toolchains))

(packages->manifest
 (append
  (list bash-minimal
        coreutils-minimal
        findutils
        gawk
        grep
        gzip
        sed
        tar
        which
        cmake-minimal
        gnu-make
        git-minimal)
  (let ((target (getenv "HOST")))
    (cond ((string-contains target "-linux-")
           (list (make-linux-cross-toolchain target)))
          (else '())))))
