#include "usdk_test.h"
#include "usdk/manifest.h"

static void add(usdk_manifest_set_t* set, const char* json) {
    usdk_manifest_t m;
    char detail[128];
    usdk_status_t st = usdk_manifest_parse((const uint8_t*)json, (uint32_t)strlen(json), &m, detail, sizeof(detail));
    USDK_CHECK_EQ_INT(st, USDK_OK);
    set->entries[set->count++] = m;
}

static int index_of(const usdk_resolution_plan_t* plan, const char* name) {
    for (uint32_t i = 0; i < plan->count; ++i) if (strcmp(plan->order[i]->name, name) == 0) return (int)i;
    return -1;
}

USDK_TEST_MAIN_BEGIN()
    /* --- parse: fields, including a dependency array. ----------------- */
    {
        usdk_manifest_t m;
        char detail[128];
        const char* json =
            "{\"name\":\"usdk-deliberate\",\"version_major\":0,\"version_minor\":1,"
            "\"role\":\"deliberate\",\"artifact_path\":\"usdk_deliberate\","
            "\"dependencies\":[{\"capability\":\"usdk-driver-fixture\",\"min_version_major\":0,\"min_version_minor\":1}]}";
        usdk_status_t st = usdk_manifest_parse((const uint8_t*)json, (uint32_t)strlen(json), &m, detail, sizeof(detail));
        USDK_CHECK_EQ_INT(st, USDK_OK);
        USDK_CHECK_STR_EQ(m.name, "usdk-deliberate");
        USDK_CHECK_EQ_INT(m.version_major, 0);
        USDK_CHECK_EQ_INT(m.version_minor, 1);
        USDK_CHECK_EQ_INT(m.role, USDK_ROLE_DELIBERATE);
        USDK_CHECK_STR_EQ(m.artifact_path, "usdk_deliberate");
        USDK_CHECK_EQ_INT(m.dependency_count, 1);
        USDK_CHECK_STR_EQ(m.dependencies[0].capability, "usdk-driver-fixture");
    }

    /* --- parse: missing required field is rejected, not defaulted. ---- */
    {
        usdk_manifest_t m;
        char detail[128];
        const char* json = "{\"version_major\":0,\"version_minor\":1,\"role\":\"perceive\",\"artifact_path\":\"x\"}";
        usdk_status_t st = usdk_manifest_parse((const uint8_t*)json, (uint32_t)strlen(json), &m, detail, sizeof(detail));
        USDK_CHECK_EQ_INT(st, USDK_ERR_INVALID_ARGUMENT);
    }

    /* --- resolve: a real 3-node diamond (A depends on B and C; C also
     * depends on B) - B must come before both A and C, and the whole
     * transitive graph is discovered from a request naming only A. ---- */
    {
        usdk_manifest_set_t set; memset(&set, 0, sizeof(set));
        add(&set, "{\"name\":\"B\",\"version_major\":1,\"version_minor\":0,\"role\":\"driver\",\"artifact_path\":\"b\",\"dependencies\":[]}");
        add(&set, "{\"name\":\"C\",\"version_major\":1,\"version_minor\":0,\"role\":\"driver\",\"artifact_path\":\"c\","
                   "\"dependencies\":[{\"capability\":\"B\",\"min_version_major\":1,\"min_version_minor\":0}]}");
        add(&set, "{\"name\":\"A\",\"version_major\":1,\"version_minor\":0,\"role\":\"driver\",\"artifact_path\":\"a\","
                   "\"dependencies\":[{\"capability\":\"B\",\"min_version_major\":1,\"min_version_minor\":0},"
                                     "{\"capability\":\"C\",\"min_version_major\":1,\"min_version_minor\":0}]}");
        const char* req[1] = { "A" };
        usdk_resolution_plan_t plan;
        char detail[128];
        usdk_status_t st = usdk_manifest_resolve(&set, req, 1, &plan, detail, sizeof(detail));
        USDK_CHECK_EQ_INT(st, USDK_OK);
        USDK_CHECK_EQ_INT(plan.count, 3);
        int ib = index_of(&plan, "B"), ic = index_of(&plan, "C"), ia = index_of(&plan, "A");
        USDK_CHECK(ib >= 0 && ic >= 0 && ia >= 0);
        USDK_CHECK(ib < ic); /* B before C, since C depends on B */
        USDK_CHECK(ib < ia); /* B before A */
        USDK_CHECK(ic < ia); /* C before A */
    }

    /* --- resolve: a genuine cycle is detected, not silently accepted or
     * "solved" by picking a shortest path through it. ------------------ */
    {
        usdk_manifest_set_t set; memset(&set, 0, sizeof(set));
        add(&set, "{\"name\":\"X\",\"version_major\":1,\"version_minor\":0,\"role\":\"driver\",\"artifact_path\":\"x\","
                   "\"dependencies\":[{\"capability\":\"Y\",\"min_version_major\":1,\"min_version_minor\":0}]}");
        add(&set, "{\"name\":\"Y\",\"version_major\":1,\"version_minor\":0,\"role\":\"driver\",\"artifact_path\":\"y\","
                   "\"dependencies\":[{\"capability\":\"X\",\"min_version_major\":1,\"min_version_minor\":0}]}");
        const char* req[1] = { "X" };
        usdk_resolution_plan_t plan;
        char detail[128];
        usdk_status_t st = usdk_manifest_resolve(&set, req, 1, &plan, detail, sizeof(detail));
        USDK_CHECK_EQ_INT(st, USDK_ERR_DEPENDENCY_CYCLE);
    }

    /* --- resolve: a dependency with no matching manifest is unresolved,
     * not silently skipped. --------------------------------------------- */
    {
        usdk_manifest_set_t set; memset(&set, 0, sizeof(set));
        add(&set, "{\"name\":\"P\",\"version_major\":1,\"version_minor\":0,\"role\":\"driver\",\"artifact_path\":\"p\","
                   "\"dependencies\":[{\"capability\":\"does-not-exist\",\"min_version_major\":1,\"min_version_minor\":0}]}");
        const char* req[1] = { "P" };
        usdk_resolution_plan_t plan;
        char detail[128];
        usdk_status_t st = usdk_manifest_resolve(&set, req, 1, &plan, detail, sizeof(detail));
        USDK_CHECK_EQ_INT(st, USDK_ERR_DEPENDENCY_UNRESOLVED);
    }

    /* --- resolve: a matching-name dependency whose version is below the
     * declared minimum is rejected, not substituted anyway. ------------ */
    {
        usdk_manifest_set_t set; memset(&set, 0, sizeof(set));
        add(&set, "{\"name\":\"OldDep\",\"version_major\":0,\"version_minor\":5,\"role\":\"driver\",\"artifact_path\":\"d\",\"dependencies\":[]}");
        add(&set, "{\"name\":\"Q\",\"version_major\":1,\"version_minor\":0,\"role\":\"driver\",\"artifact_path\":\"q\","
                   "\"dependencies\":[{\"capability\":\"OldDep\",\"min_version_major\":1,\"min_version_minor\":0}]}");
        const char* req[1] = { "Q" };
        usdk_resolution_plan_t plan;
        char detail[128];
        usdk_status_t st = usdk_manifest_resolve(&set, req, 1, &plan, detail, sizeof(detail));
        USDK_CHECK_EQ_INT(st, USDK_ERR_DEPENDENCY_VERSION_INCOMPATIBLE);
    }
USDK_TEST_MAIN_END()
