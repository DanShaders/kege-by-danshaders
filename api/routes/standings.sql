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
SELECT DISTINCT ON (task_id)
    task_id,
    users_answers.user_id,
    score
FROM (users_groups
    JOIN users_answers ON users_groups.user_id = users_answers.user_id)
WHERE
    group_id = `group_id`
    AND kim_id = `kim_id`
ORDER BY
    task_id,
    submit_time DESC;

-- Get readable usernames
SELECT
    id,
    display_name
FROM (users
    JOIN unnest(`users`::bigint[]) AS ids ON id = ids);

