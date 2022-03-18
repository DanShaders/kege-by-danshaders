CREATE SEQUENCE seq_id;

CREATE TYPE grading_policy AS ENUM ('full_match', 'independent_check');

CREATE TABLE ege_types (
	id integer DEFAULT nextval('seq_id') NOT NULL PRIMARY KEY,
	obsolete bool,
	short_name text,
	full_name text,
	grading grading_policy,
	scale_factor double precision CHECK (scale_factor > 0)
);

CREATE TABLE tasks (
	id integer DEFAULT nextval('seq_id') NOT NULL PRIMARY KEY,
	ege_type integer,
	parent integer,
	task text,
	source text,
	answer_rows integer,
	answer_cols integer,
	answer bytea,

	FOREIGN KEY (ege_type) REFERENCES ege_types(id),
	FOREIGN KEY (parent) REFERENCES tasks(id)
);

CREATE TABLE task_attachments (
	id integer DEFAULT nextval('seq_id') NOT NULL PRIMARY KEY,
	task_id integer,
	orig_filename text,
	fs_filename text,

	FOREIGN KEY (task_id) REFERENCES tasks(id) ON DELETE CASCADE
);

CREATE TABLE task_images (
	id integer DEFAULT nextval('seq_id') NOT NULL PRIMARY KEY,
	task_id integer,
	mime_type text,
	fs_filename text,

	FOREIGN KEY (task_id) REFERENCES tasks(id) ON DELETE CASCADE
);

CREATE TABLE kims (
	id integer NOT NULL PRIMARY KEY,
	duration integer,
	virtual boolean,
	exam boolean,

	start_time timestamp,
	end_time timestamp,

	CHECK ((end_time IS NULL) != (virtual = true))
);

CREATE TABLE kims_tasks (
	kim_id integer,
	task_id integer,
	pos integer,
	name text,

	FOREIGN KEY (kim_id) REFERENCES kims(id) ON DELETE CASCADE,
	FOREIGN KEY (task_id) REFERENCES tasks(id),
	UNIQUE (kim_id, pos)
);

CREATE TABLE users (
	id integer DEFAULT nextval('seq_id') NOT NULL PRIMARY KEY, 
	username text,
	display_name text,
	permissions integer,
	salt char(64),
	password char(64), -- = b16encode(sha3_256(password + b16decode(salt)))
	last_login_time timestamp,
	last_login_method text
);

CREATE TABLE sessions (
	id char(64) NOT NULL PRIMARY KEY,
	user_id integer,
	login_time bigint, 

	FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE groups (
	id integer DEFAULT nextval('seq_id') NOT NULL PRIMARY KEY,
	display_name text
);

CREATE TABLE users_groups (
	user_id integer,
	group_id integer,

	FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
	FOREIGN KEY (group_id) REFERENCES groups(id) ON DELETE CASCADE
);

CREATE TABLE groups_kims (
	group_id integer,
	kim_id integer,

	FOREIGN KEY (group_id) REFERENCES groups(id) ON DELETE CASCADE,
	FOREIGN KEY (kim_id) REFERENCES kims(id) ON DELETE CASCADE
);

CREATE TABLE users_kims (
	user_id integer,
	kim_id integer,
	start_time timestamp,
	end_time timestamp,

	FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
	FOREIGN KEY (kim_id) REFERENCES kims(id) ON DELETE CASCADE
);

CREATE TABLE users_answers (
	kim_id integer,
	pos integer,
	user_id integer,
	answer bytea,

	FOREIGN KEY (kim_id, pos) REFERENCES kims_tasks(kim_id, pos) ON DELETE CASCADE,
	FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE users_results (
	kim_id integer,
	user_id integer,
	points integer[],
	total integer,

	FOREIGN KEY (kim_id) REFERENCES kims(id) ON DELETE CASCADE,
	FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE standings (
	kim_id integer,
	group_id integer,
	name text,
	fs_filename text,
	creation_time timestamp,

	FOREIGN KEY (kim_id) REFERENCES kims(id) ON DELETE CASCADE,
	FOREIGN KEY (group_id) REFERENCES groups(id) ON DELETE CASCADE
);

-- admin:password
INSERT INTO users (username, display_name, permissions, salt, password) VALUES (
	'admin',
	'Administrator',
	2147483647,
	'8a339ca48b640a58dfac3c8f40963e7ed99ad0f525146f8c1b6a14fe6b07adf7',
	'4db6fac3b8811822e57a926e9ff52b5e1aea0bd52e58125e0d5fccc45d0c0057'
);

--user:password
INSERT INTO users (username, display_name, permissions, salt, password) VALUES (
	'user',
	'User',
	0,
	'a93c87f2f88b9aca33ba64fdb35bde919c0aadfd690507d9b2da38cb1a9ba4ff',
	'e1bdae5584062bf6a4472260180be513e1db4cfe334fec1c67cb5e5eb26452fa'
);
