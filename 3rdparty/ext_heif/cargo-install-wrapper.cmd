@echo off

: Due to a Cargo bug, `cargo install` might not work on the first try.
: Retry once if it failed on the first try.
: See: https://github.com/rust-lang/cargo/issues/9410`

cargo install %* || (echo Retrying cargo install a second time... & cargo install %*)
