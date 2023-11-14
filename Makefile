rebuild: clean build
build: style_css.h offsets.h svgs.h

clean:
	rm style_css.h offsets.h svgs.h

style_css.h:
	bin/generate_css.py html_assets/style.css > $@

offsets.h:
	bin/generate_offsets.py > $@

svgs.h:
	bin/generate_svgs.py $(wildcard html_assets/*.svg) > $@
