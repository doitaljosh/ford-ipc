# Global Ford Antenna typography update — 2026-10-03

- All runtime UI text now uses the extracted Ford Antenna Cond WGL family.
- Ford Antenna Cond Light is the default face for normal/inactive text.
- Ford Antenna Cond Regular is the emphasized/bold face.
- Traffic-sign numerals/units now use Ford Antenna Cond Regular as well, so there is no alternate runtime typeface.
- PRNDL now renders only the active range in bold Ford Antenna Regular and Ford orange `#FF9600`; inactive ranges use Antenna Light in dim gray.
- No system-font fallback is permitted. Missing Ford Antenna files remain a startup error.
- The project still does not redistribute the proprietary font binaries; use `extract_cluster_fonts.py` against the user's IPC firmware.
