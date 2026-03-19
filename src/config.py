"""src/config.py — Load settings.yaml into typed dataclasses."""

import yaml
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class DuckDBConfig:
    path: str


@dataclass
class OracleConfig:
    mock_db: str = "data/mock_oracle.db"


@dataclass
class GitRepoConfig:
    path: str
    name: str


@dataclass
class GitConfig:
    repos: list = field(default_factory=list)


@dataclass
class Settings:
    duckdb: DuckDBConfig
    oracle: OracleConfig
    git: GitConfig


def _load(path: str = "config/settings.yaml") -> Settings:
    with open(path, encoding="utf-8") as f:
        raw = yaml.safe_load(f)
    return Settings(
        duckdb=DuckDBConfig(path=raw["duckdb"]["path"]),
        oracle=OracleConfig(mock_db=raw.get("oracle", {}).get("mock_db", "data/mock_oracle.db")),
        git=GitConfig(repos=[GitRepoConfig(**r) for r in raw.get("git", {}).get("repos", [])]),
    )


settings = _load()
