use anyhow::Result;
use vergen::{vergen, Config, ShaKind, TimeZone};

fn main() -> Result<()> {
    let mut config = Config::default();
    *config.build_mut().timezone_mut() = TimeZone::Local;
    *config.git_mut().sha_kind_mut() = ShaKind::Short;
    *config.git_mut().semver_dirty_mut() = Some("-dirty");
    *config.git_mut().commit_timestamp_timezone_mut() = TimeZone::Local;
    vergen(config)
}
