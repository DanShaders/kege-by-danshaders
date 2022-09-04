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

