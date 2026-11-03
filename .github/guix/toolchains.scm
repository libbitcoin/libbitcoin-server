;; Copyright (c) 2014-2026 libbitcoin-server developers (see COPYING).
;; Derived from Bitcoin Core contrib/guix/toolchains.scm,
;; Copyright (c) The Bitcoin Core developers, MIT license
;; (https://opensource.org/license/mit).

(define-module (toolchains)
  #:use-module (gnu packages)
  #:use-module (gnu packages base)
  #:use-module (gnu packages cross-base)
  #:use-module (gnu packages gcc)
  #:use-module ((gnu packages linux) #:select (linux-libre-headers-6.1))
  #:use-module (guix build-system trivial)
  #:use-module (guix download)
  #:use-module (guix gexp)
  #:use-module (guix git-download)
  #:use-module (guix packages)
  #:use-module ((guix utils) #:select (substitute-keyword-arguments))
  #:export (make-linux-cross-toolchain))

(define %our-patches
  (string-append (dirname (search-path %load-path "toolchains.scm")) "/patches"))

(define-syntax-rule (search-our-patches file-name ...)
  "Return the list of absolute file names corresponding to each
FILE-NAME found in ./patches relative to this module."
  (parameterize ((%patch-path (list %our-patches)))
    (list (search-patch file-name) ...)))

(define building-on
  (string-append "--build="
                 (list-ref (string-split (%current-system) #\-) 0)
                 "-guix-linux-gnu"))

(define (base-binutils target)
  (package
    (inherit (cross-binutils target))
    (version "2.46.0")
    (source (origin
              (method url-fetch)
              (uri (string-append "mirror://gnu/binutils/binutils-"
                                  version ".tar.bz2"))
              (sha256
               (base32
                "04nd9vl7c1pxjbc9wh3ckddzhz5g82xyjqq9y9kf171a59im4c8g"))))
    (arguments
      (substitute-keyword-arguments (package-arguments (cross-binutils target))
        ((#:configure-flags flags)
          #~(append #$flags
            (list "--enable-gprofng=no")))))
    (native-inputs
      (modify-inputs
        (package-native-inputs (cross-binutils target))
        (delete "bison")))))

(define base-gcc
  (package-with-extra-patches gcc-14
    (search-our-patches "gcc-remap-guix-store.patch" "gcc-ssa-generation.patch")))

(define linux-base-gcc
  (package
    (inherit base-gcc)
    (arguments
      (substitute-keyword-arguments (package-arguments base-gcc)
        ((#:configure-flags flags)
           #~(append #$flags
            (list "--enable-cet=yes"
                  "--enable-default-ssp=yes"
                  "--enable-default-pie=yes"
                  "--enable-gprofng=no"
                  "--enable-host-bind-now=yes"
                  "--enable-initfini-array=yes"
                  "--enable-standard-branch-protection=yes"
                  "--disable-gcov"
                  "--disable-libgomp"
                  "--disable-libquadmath"
                  "--disable-libsanitizer"
                  "--disable-lto"
                  "--disable-nls"
                  "--disable-tm-clone-registry"
                  #$building-on)))
        ((#:phases phases)
          #~(modify-phases #$phases
            (add-after 'pre-configure 'replace-rpath-with-rpath-link
             (lambda _
               (substitute* (cons "gcc/config/rs6000/sysv4.h"
                                  (find-files "gcc/config"
                                              "^gnu-user.*\\.h$"))
                 (("-rpath=") "-rpath-link="))
               #t))))))))

(define glibc-2.31
  (let ((commit "28eb5caf895ced5d895cb02757e109004a2d33e5"))
  (package
    (inherit glibc)
    (version "2.31")
    (source (origin
              (method git-fetch)
              (uri (git-reference
                    (url "https://sourceware.org/git/glibc.git")
                    (commit commit)))
              (file-name (git-file-name "glibc" commit))
              (sha256
               (base32
                "07arjrc1smqy8wrhg38apr1s9ji7xv1rpzdapk4k2ps2n07irp58"))
              (patches (search-our-patches "glibc-guix-prefix.patch"))))
    (arguments
      (substitute-keyword-arguments (package-arguments glibc)
        ((#:configure-flags flags)
          `(append ,flags
            (list "--enable-bind-now",
                  "--enable-cet",
                  "--enable-kernel=3.17.0",
                  "--enable-stack-protector=all",
                  "--disable-profile",
                  "--disable-timezone-tools",
                  "--disable-werror",
                  building-on)))
    ((#:phases phases)
        `(modify-phases ,phases
           (add-before 'configure 'set-etc-rpc-installation-directory
             (lambda* (#:key outputs #:allow-other-keys)
               (let ((out (assoc-ref outputs "out")))
                 (substitute* "sunrpc/Makefile"
                   (("^\\$\\(inst_sysconfdir\\)/rpc(.*)$" _ suffix)
                    (string-append out "/etc/rpc" suffix "\n"))
                   (("^install-others =.*$")
                    (string-append "install-others = " out "/etc/rpc\n")))))))))))))

(define (make-linux-cross-toolchain target)
  "Return a gcc 14 / glibc 2.31 cross toolchain package for TARGET."
  (let* ((xbinutils (base-binutils target))
         (xgcc-sans-libc (cross-gcc target
                                    #:xgcc linux-base-gcc
                                    #:xbinutils xbinutils))
         (xkernel (cross-kernel-headers target
                                        #:linux-headers linux-libre-headers-6.1
                                        #:xgcc xgcc-sans-libc
                                        #:xbinutils xbinutils))
         (xlibc (cross-libc target
                            #:libc glibc-2.31
                            #:xgcc xgcc-sans-libc
                            #:xbinutils xbinutils
                            #:xheaders xkernel))
         (xgcc (cross-gcc target
                          #:xgcc linux-base-gcc
                          #:xbinutils xbinutils
                          #:libc xlibc)))
    (package
      (name (string-append target "-toolchain"))
      (version (package-version xgcc))
      (source #f)
      (build-system trivial-build-system)
      (arguments '(#:builder (begin (mkdir %output) #t)))
      (propagated-inputs
        (list xbinutils
              xlibc
              xgcc
              `(,xlibc "static")
              `(,xgcc "lib")))
      (synopsis (string-append "Complete GCC tool chain for " target))
      (description (string-append "This package provides a complete GCC tool
chain for " target " development."))
      (home-page (package-home-page xgcc))
      (license (package-license xgcc)))))
