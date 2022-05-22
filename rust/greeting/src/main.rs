#![feature(once_cell)]

use futures::future::select;
use futures::pin_mut;
use hyper::service::{make_service_fn, service_fn};
use hyper::{Body, Method, Request, Response, Server, StatusCode};
use std::lazy::SyncLazy;
use structopt::StructOpt;
use tokio::signal;
use urlencoding::decode;

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
    name = "greeting",
    about = "a small server for testing rust's performance",
    long_version = &**VERSION_INFO,
)]
pub struct CommandLineArgs {
    /// The address to bind for listening.
    #[structopt(long = "binding_address", default_value = "0.0.0.0:3000")]
    pub binding_address: String,

    /// Tokio worker threads.
    #[structopt(long = "tokio_worker_threads", default_value = "16")]
    pub tokio_worker_threads: usize,
}

fn greeting(name: &str) -> String {
    format!("Hello {}\n", name)
}

async fn greeting_service(req: Request<Body>) -> Result<Response<Body>, hyper::Error> {
    if req.method() != Method::GET || req.uri().path() != "/greeting" {
        let mut res = Response::default();
        *res.status_mut() = StatusCode::NOT_FOUND;
        return Ok(res);
    }

    let name = req
        .uri()
        .query()
        .iter()
        .flat_map(|s| s.split('&'))
        .flat_map(|s| s.split_once('='))
        .filter_map(|(k, v)| if k == "name" { decode(v).ok() } else { None })
        .last();

    match name {
        Some(name) => {
            let mut res = Response::new(Body::from(greeting(&name)));
            res.headers_mut()
                .insert("Content-Type", "text/html; charset=utf-8".parse().unwrap());
            Ok(res)
        }
        None => {
            let mut res = Response::default();
            *res.status_mut() = StatusCode::BAD_REQUEST;
            Ok(res)
        }
    }
}

fn main() {
    let args = CommandLineArgs::from_args();

    let rt = tokio::runtime::Builder::new_multi_thread()
        .worker_threads(args.tokio_worker_threads)
        .enable_all()
        .build()
        .unwrap();

    rt.block_on(async {
        let args = CommandLineArgs::from_args();
        let service =
            make_service_fn(|_| async { Ok::<_, hyper::Error>(service_fn(greeting_service)) });
        let server = Server::bind(&args.binding_address.parse().unwrap()).serve(service);
        let shutdown = signal::ctrl_c();
        pin_mut!(shutdown);
        pin_mut!(server);

        select(shutdown, server).await;
    });
}
