-- Available KIMs
--  nullable virtual_start_time
--  nullable virtual_end_time
SELECT
    kims.id,
    name,
    token_version,
    groups_kims.start_time,
    groups_kims.end_time,
    duration,
    virtual,
    exam,
    users_kims.start_time AS virtual_start_time,
    users_kims.end_time AS virtual_end_time
FROM (users_groups
    JOIN groups_kims ON users_groups.group_id = groups_kims.group_id
    JOIN kims ON groups_kims.kim_id = kims.id
    LEFT JOIN users_kims ON users_kims.kim_id = kims.id
        AND users_kims.user_id = users_groups.user_id)
WHERE
    users_groups.user_id = `user_id`
    AND NOT coalesce(deleted, FALSE)
ORDER BY
    exam,
    kims.id DESC;

-- Get KIM tasks
SELECT
    id,
    task_type,
    task,
    answer_rows,
    answer_cols
FROM (kims_tasks
    JOIN tasks ON task_id = id)
WHERE
    kim_id = `id`
ORDER BY
    pos;

-- Get KIM attachments
SELECT
    task_id,
    filename,
    hash,
    mime_type
FROM
    task_attachments
    JOIN unnest(`task_ids`::bigint[]) AS ids ON ids = task_id
WHERE
    shown_to_user
    AND NOT coalesce(deleted, FALSE);

-- Start virtual
INSERT INTO users_kims
    VALUES (`session->user_id`, `kim->id`, `*kim->virtual_start_time`, `*kim->virtual_end_time`);

-- Get user answers
SELECT DISTINCT ON (task_id)
    task_id,
    answer
FROM
    users_answers
WHERE
    user_id = `session->user_id`
    AND kim_id = `id`
ORDER BY
    task_id,
    submit_time DESC;

-- Get token version
SELECT
    token_version
FROM
    kims
WHERE
    id = `token.data.kim_id`;

-- Get real answer
SELECT
    answer,
    grading,
    scale_factor
FROM (tasks
    JOIN task_types ON tasks.task_type = task_types.id)
WHERE
    tasks.id = `req.task_id()`;

-- Write answer
INSERT INTO users_answers
    VALUES (`token.data.kim_id`, `req.task_id()`, `session->user_id`, `req.answer()`, `score`, `current_millis`);

-- End participation
INSERT INTO users_kims
    VALUES (`session->user_id`, `req.id()`, NULL, CURRENT_TIMESTAMP)
ON CONFLICT (user_id, kim_id)
    DO UPDATE SET
        end_time = excluded.end_time;

