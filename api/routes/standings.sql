-- Get KIM tasks
SELECT
    tasks.id,
    short_name
FROM (kims_tasks
    JOIN tasks ON kims_tasks.task_id = tasks.id
    JOIN task_types ON tasks.task_type = task_types.id)
WHERE
    kim_id = `kim_id`
ORDER BY
    pos;

-- Get user answers
SELECT DISTINCT ON (task_id, user_id)
    id,
    task_id,
    users_answers.user_id,
    score,
    submit_time
FROM (users_groups
    JOIN users_answers ON users_groups.user_id = users_answers.user_id)
WHERE
    group_id = `group_id`
    AND kim_id = `kim_id`
    AND id >= `min_id`
ORDER BY
    task_id,
    user_id,
    submit_time DESC;

-- Get readable usernames
SELECT
    id,
    display_name
FROM (users
    JOIN unnest(`users`::bigint[]) AS ids ON id = ids);

-- Get users of group
SELECT
    user_id,
    display_name
FROM (users
    JOIN users_groups ON users.id = user_id)
WHERE
    group_id = `group_id`;

-- Get user submissions
SELECT
    score,
    submit_time,
    answer
FROM
    users_answers
WHERE
    user_id = `request.user_id()`
    AND task_id = `request.task_id()`
    AND kim_id = `request.kim_id()`
ORDER BY
    submit_time DESC;

