# Test the actual OWE countdown while sharing the ordinary native objects.
.PHONY: wild-spawn-interval-tests
wild-spawn-interval-tests: $(OBJ_DIR)/wild-spawn-interval-tests$(SUFFIX)

$(OBJ_DIR)/native/wild-spawn-interval-owe.o: src/wild_encounter_ow.c include/mod_api.h include/config/wild_encounter.h $(OBJ_DIR)/config Makefile.native native/tests/wild_spawn_interval.mk
	@mkdir -p $(@D)
	@$(CPP) -m$(BITS) $(CPPFLAGS) -DWE_OW_ENCOUNTERS=TRUE -DWE_OWE_SPAWN_REPLACEMENT=TRUE -DRHH_WILD_SPAWN_TEST $< | $(PREPROC) -ni -g $(ASSETS_DIR_NAME) $< charmap.txt | $(CC) $(CFLAGS) -x cpp-output -c - -o $@

$(OBJ_DIR)/native/wild-spawn-interval-gameplay.o: native/tests/gameplay_test.c native/tests/wild_spawn_interval_test.inc $(OBJ_DIR)/config Makefile.native native/tests/wild_spawn_interval.mk
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -DRHH_WILD_SPAWN_TEST -c $< -o $@

$(OBJ_DIR)/wild-spawn-interval-tests$(SUFFIX): $(filter-out $(OBJ_DIR)/native/platform.o $(OBJ_DIR)/src/wild_encounter_ow.o,$(OBJS)) $(OBJ_DIR)/native/platform-test.o $(OBJ_DIR)/native/wild-spawn-interval-owe.o $(OBJ_DIR)/native/wild-spawn-interval-gameplay.o $(GME_LIBRARY)
	$(CC) $(LDFLAGS) -o $@ $(filter %.o,$^) $(SDL_LIBS) $(GME_LINK) -lm

-include $(OBJ_DIR)/native/wild-spawn-interval-gameplay.d
