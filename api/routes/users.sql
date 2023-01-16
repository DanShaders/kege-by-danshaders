-- List groups
SELECT id, display_name FROM groups;

-- List all users
SELECT id, username, display_name, permissions FROM users;

-- List group mapping
SELECT user_id, group_id FROM users_groups ORDER BY group_id;

-- Add users
INSERT INTO users (username, display_name, permissions, salt, password)
VALUES (
	unnest(`usernames`::text[]),
	unnest(`display_names`::text[]),
	0,
	unnest(`salts`::text[]),
	unnest(`passwords`::text[])
) RETURNING id;

-- Drop all users
DELETE FROM users WHERE id != 1 and id != 2;

-- Drop all groups
DELETE FROM groups WHERE id != 3;

-- Add groups
INSERT INTO groups (display_name) VALUES (unnest(`group_names`::text[])) RETURNING id;

-- Add group mapping
INSERT INTO users_groups (user_id, group_id) VALUES (unnest(`map_user_id`::bigint[]), unnest(`map_group_id`::bigint[]));

