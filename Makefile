.PHONY: tags cscope

all: tags cscope

tags:
	@echo "[GEN] tags"
	@ctags -R --exclude=build

cscope:
	@echo "[GEN] cscope"
	@find . \
		\( -path "./src/*" -o -path "./include/*" \) \
		\( -name "*.[ch]" -o -name "*.cpp" \) \
		-print > $(PWD)/cscope.files
	@cscope -b -q -k

clean:
	rm cscope.* tags
