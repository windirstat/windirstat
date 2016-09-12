TEMPLATES:=$(wildcard templates/*.tmpl)
TMPLINCLS:=$(wildcard templates/include/*.tt)
HTMLPAGES:=$(patsubst templates/%.tmpl,%,$(TEMPLATES))
TPAGEFLAG:=--pre_chomp --post_chomp --trim --include_path=templates/include
AUTOMENU:=templates/include/automenu.inc
STYLESHEET:=templates/include/styles.css

all: $(HTMLPAGES)

$(AUTOMENU): $(TEMPLATES)
	(echo "[%\npages = ["; for i in $^; do FNAME="$${i##*/}"; echo " {\n   fname = '$${FNAME%.tmpl}'"; grep -E '^\s+(description|norder|uuid|title) = ' $$i; echo " }\n"; done; echo "\n]\n%]";) > $@

%: templates/%.tmpl $(AUTOMENU) $(TMPLINCLS) $(STYLESHEET)
	-@echo "$< --> $@"
	tpage $(TPAGEFLAG) $< > $@

clean:
	rm -f $(HTMLPAGES) $(AUTOMENU)
rebuild: clean all

.PHONY: all clean rebuild
.NOTPARALLEL: clean rebuild
