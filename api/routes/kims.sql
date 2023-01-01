-- Get KIM
SELECT
    *
FROM
    kims
WHERE
    id = `kim_id`;

-- Get KIM tasks
SELECT
    task_id,
    task_type,
    tag
FROM (kims_tasks
    JOIN tasks ON tasks.id = kims_tasks.task_id)
WHERE
    kim_id = `kim_id`
ORDER BY
    pos;

-- Get KIM groups
SELECT
    *
FROM
    groups_kims
WHERE
    kim_id = `kim_id`;

-- Get KIM list
SELECT
    *
FROM
    kims
WHERE
    NOT coalesce(deleted, FALSE);

-- Bump KIM version
UPDATE
    kims
SET
    token_version = COALESCE(token_version, 0) + 1
WHERE
    id = `utils::expect<int64_t>(r, "id")`;

-- Lock mentioned tasks
SELECT
    task_id,
    pos
FROM
    unnest(`task_ids`::bigint[]) AS ids
    JOIN kims_tasks ON task_id = ids
WHERE
    kim_id = `kim.id()`
ORDER BY
    (task_id,
        pos)
FOR UPDATE;

-- Insert task count record
INSERT INTO kims_tasks
    VALUES (`kim.id()`, NULL, `new_end_pos`);

-- Update task count record
UPDATE
    kims_tasks
SET
    pos = pos + `introduced_cnt`
WHERE
    kim_id = `kim.id()`
    AND task_id IS NULL
RETURNING
    pos;

-- Delete tasks
DELETE FROM kims_tasks USING unnest(`delete_ids`::bigint[]) AS ids
    WHERE kim_id = `kim.id()`
        AND task_id = ids;

-- Insert and swap tasks
INSERT INTO kims_tasks
    VALUES (`kim.id()`, unnest(`swap_ids`::bigint[]), unnest(`swap_positions`::int[]))
ON CONFLICT (kim_id, task_id)
    DO UPDATE SET
        pos = excluded.pos;

