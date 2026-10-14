/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
/* 앱과 호스트 테스트가 같은 진입·복귀 순서를 사용한다. */
static int mc_ota_entry_service(void)
{
    int rc;
    ota_service();
    if (!mc_ota_requested())
        return 0;
    if (!mc_ota_flash_known()) {
        mc_ota_refuse();
        mc_ota_result(-15);
        return 1;
    }
    mc_ota_enter();
    rc = ota_session();
    /* 실제 성공은 ota_commit에서 재부팅한다. 반환 경로는 모두 앱을 재개한다. */
    mc_ota_leave();
    mc_ota_result(rc);
    return 1;
}

static void mc_ota_boot_service(void)
{
    int rc;
    if (!flash_ok || !mc_ota_flash_known())
        return;
    mc_ota_enter();
    rc = ota_boot_cleanup();
    mc_ota_leave();
    if (rc < 0)
        mc_ota_result(rc);
}
