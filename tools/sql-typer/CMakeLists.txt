add_executable(sql-typer main.cc)

pkg_check_modules(LIBPQ REQUIRED IMPORTED_TARGET libpq)
pkg_check_modules(FMT REQUIRED IMPORTED_TARGET fmt)

target_link_libraries(
	sql-typer PRIVATE
	PkgConfig::LIBPQ
	PkgConfig::FMT
)
