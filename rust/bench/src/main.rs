#![feature(once_cell)]

use futures::future::{join_all, select};
use futures::pin_mut;
use hyper::{body::HttpBody as _, client::HttpConnector, http::Uri, Client, StatusCode};
use lazy_static::lazy_static;
use log::{error, info};
use prometheus::{self, Encoder, Histogram, IntCounter, TextEncoder};
use prometheus::{register_histogram, register_int_counter};
use rand::rngs::ThreadRng;
use rand::seq::SliceRandom;
use std::future::Future;
use std::io::Write;
use std::lazy::SyncLazy;
use structopt::StructOpt;
use tokio::signal;
use tokio::sync::broadcast;
use tokio::time::{sleep, Duration};
use urlencoding::encode;

lazy_static! {
    static ref SUCCESS_COUNTER: IntCounter =
        register_int_counter!("success", "Number of success").unwrap();
    static ref FAILURE_COUNTER: IntCounter =
        register_int_counter!("failure", "Number of failure").unwrap();
    static ref GREETING_HISTOGRAM: Histogram =
        register_histogram!("greeting", "greeting histogram").unwrap();
}

static VERSION_INFO: SyncLazy<String> = SyncLazy::new(|| {
    format!(
        "version: {} {}@{} last modified at {} build at {}",
        env!("VERGEN_GIT_SEMVER"),
        env!("VERGEN_GIT_SHA_SHORT"),
        env!("VERGEN_GIT_BRANCH"),
        env!("VERGEN_GIT_COMMIT_TIMESTAMP"),
        env!("VERGEN_BUILD_TIMESTAMP"),
    )
});

#[derive(Debug, StructOpt)]
#[structopt(
    name = "bench",
    about = "a tool for benching greeting server's performance",
    long_version = &**VERSION_INFO,
)]
pub struct CommandLineArgs {
    /// The greeting server address to bench.
    #[structopt(long = "greeting_server_address", default_value = "127.0.0.1:3000")]
    pub greeting_server_address: String,

    /// Concurrent to bench.
    #[structopt(long = "concurrent", default_value = "32")]
    pub concurrent: usize,

    /// Bench time in seconds.
    #[structopt(long = "bench_secs", default_value = "300")]
    pub bench_secs: u64,

    /// Tokio worker threads.
    #[structopt(long = "tokio_worker_threads", default_value = "16")]
    pub tokio_worker_threads: usize,
}

fn main() {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info"))
        .format(|buf, record| {
            writeln!(
                buf,
                "[{} {}:{} {} {}:{}] {}",
                chrono::Local::now().format("%Y-%m-%d %H:%M:%S%.3f"),
                std::process::id(),
                gettid::gettid(),
                record.level(),
                record.file().unwrap_or("unknown"),
                record.line().unwrap_or_default(),
                record.args()
            )
        })
        .init();

    let args = CommandLineArgs::from_args();
    let rt = tokio::runtime::Builder::new_multi_thread()
        .worker_threads(args.tokio_worker_threads)
        .enable_all()
        .build()
        .unwrap();

    rt.block_on(async {
        let shutdown = signal::ctrl_c();
        pin_mut!(shutdown);
        let timer_up = sleep(Duration::from_secs(args.bench_secs));
        pin_mut!(timer_up);

        run_bench(
            args.concurrent,
            &args.greeting_server_address,
            select(shutdown, timer_up), // we will exit if got ctrl_c or bench_secs timer is up
        )
        .await;
    });
}

fn generate_request_uri(greeting_server_address: &str) -> Uri {
    const NAMES: &[&str] = &[
        "刘备",
        "关羽",
        "张飞",
        "Harry Potter",
        "Ronald Weasley",
        "Hermione Granger",
        "新垣結衣",
        "石原さとみ",
        "長澤まさみ",
    ];

    format!(
        "http://{}/greeting?name={}",
        greeting_server_address,
        encode(NAMES.choose(&mut ThreadRng::default()).unwrap())
    )
    .parse()
    .unwrap()
}

async fn perform_request(
    client: &Client<HttpConnector>,
    uri: Uri,
) -> Result<StatusCode, Box<dyn std::error::Error + Send + Sync>> {
    let _timer = GREETING_HISTOGRAM.start_timer();

    let mut res = client.get(uri).await?;

    // Read the response body and drop it away.
    while let Some(next) = res.data().await {
        let _ = next?;
    }

    Ok(res.status())
}

async fn bench_task(
    client: &Client<HttpConnector>,
    greeting_server_address: &str,
    mut notify: broadcast::Receiver<()>,
) {
    loop {
        let uri = generate_request_uri(&greeting_server_address);

        let res = tokio::select! {
            res = perform_request(client, uri) => res,
            _ = notify.recv() => {
                return;
            }
        };

        match res {
            Ok(StatusCode::OK) => {
                SUCCESS_COUNTER.inc();
                info!("success");
            }
            Ok(status_code) => {
                FAILURE_COUNTER.inc();
                error!(
                    "perform request success, but got status code {}",
                    status_code
                );
            }
            Err(err) => {
                FAILURE_COUNTER.inc();
                error!("failed to perform request: {}", err);
            }
        };
    }
}

async fn run_bench(concurrent: usize, greeting_server_address: &str, shutdown: impl Future) {
    let mut tasks = vec![];
    let (notify_shutdown, _) = broadcast::channel(1);

    for _i in 0..concurrent {
        let greeting_server_address = greeting_server_address.to_string();
        let notify = notify_shutdown.subscribe();

        let join_handle = tokio::spawn(async move {
            let client = Client::new();

            bench_task(&client, &greeting_server_address, notify).await;
        });

        tasks.push(join_handle);
    }

    shutdown.await;
    info!("shutting down");

    drop(notify_shutdown);

    join_all(tasks).await;

    let metric_families = prometheus::gather();
    let mut buffer = Vec::new();
    TextEncoder::new()
        .encode(&metric_families, &mut buffer)
        .unwrap();

    println!("{}", String::from_utf8(buffer).unwrap());
}
