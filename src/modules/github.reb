REBOL [
    version: 0.4.0
    title: "Rebol/GitHub utilities"
    name: github
    type: module
    date: 26-Oct-2026
    needs: json
    home: https://github.com/Oldes/Rebol-GitHub
    exports: [
        github-query
        github-repo
        github-run
        github-get
        github-post
        github-edit
        fail
    ]
]
comment "## Include: %compatibility.reb"
comment {## Title:   "Backward compatibility"}
if system/version < 3.22.5 [
    fail: func [
        {Raises the given error or an error with the given message.}
        value [string! error!] "error message or error value"
    ] [
        do make error! value
    ]
]
unless any-function? :print-hline [
    print-hline: :print-horizontal-line
]
comment "-- End of:  %compatibility.reb"
comment "## Include: %github-rest.reb"
comment {## Title:   "GitHub REST API"}
rest: context [
    api.github: https://api.github.com/
    owner:
    repository: none
    data: make map! 4
    response: none
    max-attempts: 10
    retry-delay: 0:00:02
    use-repo: func [
        "Sets the repository used by REST calls"
        o [any-word! any-string!] "Owner"
        r [any-word! any-string!] "Repository name"
    ] [owner: o repository: r]
    get: object [
        issue: func [
            "Gets an issue by its number"
            number [integer!]
        ] [
            *do 'GET [%repos/ owner %/ repository %/issues/ number] none
        ]
        issues: func [
            "Lists issues (and pull requests) in a repository."
            /part limit [integer!] "Limits number of received issues to a given length"
        ] [
            *get-pages ajoin [%repos/ owner %/ repository %/issues] none limit
        ]
        issue-comments: func [
            "Gets all comments of an issue by its number"
            number [integer!]
        ] [
            *do 'GET [%repos/ owner %/ repository %/issues/ number %/comments] none
        ]
        issue-labels: func [
            "Gets all labels of an issue by its number"
            number [integer!]
        ] [
            *do 'GET [%repos/ owner %/ repository %/issues/ number %/labels] none
        ]
        current-user: func [
            "Gets the authenticated user"
        ] [
            *do 'GET %user none
        ]
        workflows: func [
            "Lists the workflows in a repository."
        ] [
            *do 'GET [%repos/ owner %/ repository %/actions/workflows] none
        ]
        artifacts: func [
            "Lists all artifacts for a repository."
            /part limit [integer!] {Limits number of received artifacts to a given length}
        ] [
            *get-pages ajoin [%repos/ owner %/ repository %/actions/artifacts] 'artifacts limit
        ]
        page-views: func [
            {Get the total number of views and breakdown per day or week for the last 14 days.}
            /week "Breakdown per week (default: per day)"
        ] [
            *do 'GET [
                %repos/ owner %/ repository
                %/traffic/views?per= either/only week %week %day
            ] none
        ]
        popular-paths: func [
            {Get the top 10 popular contents over the last 14 days.}
        ] [
            *do 'GET [%repos/ owner %/ repository %/traffic/popular/paths] none
        ]
        popular-referrers: func [
            "Get the top 10 referrers over the last 14 days."
        ] [
            *do 'GET [%repos/ owner %/ repository %/traffic/popular/referrers] none
        ]
        contributors: func [
            {Returns the total number of commits authored by the contributor}
        ] [
            *do 'GET [%repos/ owner %/ repository %/stats/contributors] none
        ]
        emojis: func [
            "Lists all the emojis available to use on GitHub."
        ] [
            *do 'GET %emojis none
        ]
        pages: func [
            "Gets information about a GitHub Pages site."
        ] [
            *do 'GET [%repos/ owner %/ repository %/pages] none
        ]
    ]
    post: object [
        issue: func [
            "Creates an issue"
            data [map!] "title, body, labels etc.."
        ] [
            if any-string? data/labels [
                data: copy data
                data/labels: reduce [data/labels]
            ]
            *do 'POST [%repos/ owner %/ repository %/issues] data
        ]
        issue-comment: func [
            "Adds a comment to an issue by its number"
            number [integer!]
            body [string!]
        ] [
            clear data
            data/body: body
            *do 'POST [%repos/ owner %/ repository %/issues/ number %/comments] data
        ]
        issue-label: func [
            "Adds a label(s) to an issue by its number"
            number [integer!]
            label [any-string! block!]
        ] [
            clear data
            append data/labels: clear [] label
            *do 'POST [%repos/ owner %/ repository %/issues/ number %/labels] data
        ]
        label: func [
            "Creates a label"
            name [string!]
            desc [string!]
            color [string!]
        ] [
            clear data
            data/name: name
            data/description: desc
            data/color: color
            *do 'POST [%repos/ owner %/ repository %/labels] data
        ]
        release: func [
            "Creates a release"
            tag_name [string!] "Required. The name of the tag."
            target [string!] {Specifies the commitish value that determines where the Git tag is created from. Can be any branch or commit SHA. Unused if the Git tag already exists.}
            name [string!] "The name of the release."
            body [string! none!] "Text describing the contents of the tag."
            draft [logic!] {true to create a draft (unpublished) release, false to create a published one.}
            prerelease [logic!] {true to identify the release as a prerelease. false to identify the release as a full release.}
        ] [
            clear data
            data/tag_name: tag_name
            data/target_commitish: target
            data/name: name
            if body [data/body: body]
            data/draft: draft
            data/prerelease: prerelease
            *do 'POST [%repos/ owner %/ repository %/releases] data
        ]
        markdown: func [
            "Render a Markdown document"
            text [string!] "The Markdown text to render in HTML."
            /context
            repo [any-string! path!] {The repository context to use when creating references.}
        ] [
            clear data
            data/text: text
            if context [
                data/mode: "gfm"
                data/context: form repo
            ]
            *do 'POST %markdown data
        ]
    ]
    edit: object [
        issue: func [
            "Updates an issue by its number"
            number [integer!]
            data [map!] "Fields to change (title, body, state, labels etc.)"
        ] [
            *do 'PATCH [%repos/ owner %/ repository %/issues/ number] data
        ]
    ]
    run: object [
        workflow: func [
            "Triggers a workflow_dispatch event for a workflow."
            id [integer! string!] "Workflow ID or file name (e.g. main.yml)"
            /ref branch [any-string!] "Git reference (branch or tag); default: main"
            /with inputs [map!] {Input keys and values configured in the workflow file}
        ] [
            clear data
            data/ref: any [branch @main]
            data/inputs: any [inputs make map! 4]
            *do 'POST [%repos/ owner %/ repository %/actions/workflows/ id %/dispatches] data
        ]
    ]
    *send: func [
        {Sends an HTTP request and returns [status headers body]}
        url [url!]
        request [block!]
    ] [
        write/all url request
    ]
    *do: func [
        {Sends a REST request and returns the decoded response body}
        method [word!] "HTTP method"
        path [block! file!] "API path (block is joined)"
        data [map! none!] "JSON payload"
        /local header tries
    ] [
        header: *header
        if map? data [
            header/Content-Type: "application/json"
            data: to-json data
        ]
        if block? path [path: ajoin path]
        tries: max-attempts
        while [
            response: *send api.github/:path reduce [method header data]
            all [response/1 == 202 0 < tries: tries - 1]
        ] [wait retry-delay]
        *response-body
    ]
    *header: function [
        "Returns request headers with the GitHub token"
    ] [
        header: clear #[]
        unless header/Authorization: user's github-token [
            fail "Authorization token (github-token) is missing!"
        ]
        header/Accept: "application/vnd.github+json"
        header/X-GitHub-Api-Version: "2022-11-28"
        header
    ]
    *next-page: func [
        {Follows the `next` link of the last response; returns its body or none}
        /local links link url rel
    ] [
        links: clear #[]
        if all [
            string? link: select response/2 'link
            parse link [
                any [
                    thru #"<" copy url: to #">"
                    thru {; rel="} copy rel: to {"} (
                        rel: to word! rel
                        links/:rel: as url! url
                    )
                ] to end
            ]
            links/next
        ] [
            response: *send links/next reduce ['GET *header]
            *response-body
        ]
    ]
    *response-body: func [
        {Returns the body of the last response, JSON decoded; raises on HTTP errors}
        /local tmp type
    ] [
        if "^A^@" == tmp: skip tail response/3 -2 [clear tmp]
        if all [
            type: select response/2 'Content-Type
            find type "json"
        ] [
            response/3: load-json response/3
        ]
        if response/1 >= 400 [
            fail ajoin [
                "GitHub API error " response/1 ": "
                any [all [map? response/3 response/3/message] response/3]
            ]
        ]
        response/3
    ]
    *get-pages: func [
        "GETs a paginated list, following `next` links"
        path [any-string!]
        field [word! none!] {Field holding the list, or none when the body is the list}
        limit [integer! none!] "Max number of items"
        /local result tmp
    ] [
        append path ajoin [
            either find path #"?" [#"&"] [#"?"]
            "per_page=" min 100 max 1 any [limit 100]
        ]
        result: make block! 100
        response: *send api.github/:path reduce ['GET *header]
        tmp: *response-body
        while [
            if field [tmp: all [map? tmp tmp/:field]]
            block? tmp
        ] [
            append result tmp
            unless any [none? limit limit > length? result] [break]
            tmp: *next-page
        ]
        if all [limit limit < length? result] [clear skip result limit]
        result
    ]
]
comment "-- End of:  %github-rest.reb"
comment "## Include: %github-graphql.reb"
comment {## Title:   "GitHub GraphQL API"}
graphql: context [
    queries: object [
        comment "## Include: %queries.reb"
        comment {## Title:   "Some useful GitHub GraphQL queries"}
        last-20-closed-issues: {
  query($owner:String!,$repo:String!) {
    repository(owner:$owner, name:$repo) {
      issues(last:20, states:CLOSED) {
        edges {
          node {
            title
            url
            labels(first:5) {
              edges {
                node {
                  name
} } } } } } } } }
        repo-labels: {
  query($owner:String!,$repo:String!) {
    repository(owner:$owner, name:$repo) {
      labels(first:100){
        edges {
          node {
            name
            description
            color
} } } } } }
        repo-disk-usage: {
  query ($owner: String!, $repo: String!) {
    repository(owner: $owner, name: $repo) {
    diskUsage
} } }
        list-issues: {
    totalCount
    pageInfo {
      endCursor
    }
    edges {
      node {
        title
        url
        body
        author {login}
        closed
        timelineItems(first: 100) {
          nodes {
            __typename
            ... on IssueComment {
              createdAt
              author {
                login
              }
              body
            }
            ... on CrossReferencedEvent {
              createdAt
              actor {
                login
              }
              url
              source {
                __typename
                ... on Issue {
                  title
                  url
                  id
                }
                ... on PullRequest {
                  title
                  body
                  permalink
                }
              }
              target {
                __typename
                ... on Issue {
                  title
                  url
                  id
                }
                ... on PullRequest {
                  title
                  body
                  permalink
                }
              }
            }
            ... on ClosedEvent {
              createdAt
              actor {
                login
              }
              url
              closer {
                __typename
                ... on PullRequest {
                  number
                  title
                }
                ... on Commit {
                  committedDate
                  id
                  committer {
                    name
                  }
                  commitUrl
                  messageHeadline
                  messageBody
                }
              }
            }
            ... on RenamedTitleEvent {
              createdAt
              actor {login}
              currentTitle
              previousTitle
            }
            ... on MarkedAsDuplicateEvent {
              createdAt
              actor {login}
            }
            ... on LabeledEvent {
              createdAt
              actor {login}
              label {name}
            }
            ... on UnlabeledEvent {
              createdAt
              actor{login}
              label {name}
            }
            ... on SubscribedEvent {
              createdAt
              actor {login}
            }
            ... on UnsubscribedEvent {
              createdAt
              actor {login}
            }
            ... on CommentDeletedEvent {
              createdAt
              actor {login}
            }
            ... on LockedEvent {
              createdAt
              actor {login}
              lockReason
            }
            ... on UnlockedEvent {
              createdAt
              actor {login}
            }
            ... on ReopenedEvent {
              createdAt
              actor {
                login
              }
            }
            ... on ReferencedEvent {
              createdAt
              actor {
                login
              }
              commit {
                committedDate
                id
                committer {
                  name
                }
                commitUrl
                messageHeadline
                messageBody
              }
              commitRepository {
                id
              }
              isCrossRepository
              isDirectReference
            }
            ... on MentionedEvent {
              createdAt
              actor {
                login
              }
              id
            }
          }
        }
        labels(first: 5) {
          edges {
            node {
              name
            }
          }
        }
      }
    }
  }
        first-100-issues: replace {
  query($owner:String!,$repo:String!) {
    repository(owner:$owner, name:$repo) {
      issues(first:100) {
      #ISSUES#
} } } } "#ISSUES#" :list-issues
        next-100-issues: replace {
  query($owner:String!,$repo:String!,$after:String!) {
    repository(owner:$owner, name:$repo) {
      issues(first:100, after:$after) {
      #ISSUES#
} } } } "#ISSUES#" :list-issues
        last-10-commits: {
  query($owner:String!,$repo:String!) {
    repository(owner:$owner, name:$repo) {
      defaultBranchRef {
        target {
          ... on Commit {
            history(first: 10) {
              pageInfo {
                endCursor
                hasNextPage
              }
              edges {
                node {
                  oid
                  committedDate
                  messageHeadline
                  messageBody
} } } } } } } } }
        next-100-commits: {
  query($owner:String!,$repo:String!, $after:String!) {
    repository(owner:$owner, name:$repo) {
      defaultBranchRef {
        target {
          ... on Commit {
            history(first: 100, after: $after) {
              pageInfo {
                endCursor
                hasNextPage
              }
              edges {
                node {
                  oid
                  committedDate
                  messageHeadline
                  messageBody
} } } } } } } } }
        releases: {
  query ($owner: String!, $repo: String!) {
    repository(owner: $owner, name: $repo) {
      releases(first: 10) {
        edges {
          node {
            id
            name
            url
          }
        }
      }
    }
  }
}
        comment "-- End of:  %queries.reb"
    ]
    header: make map! [
        Accept: "application/vnd.github+json"
        X-GitHub-Api-Version: "2022-11-28"
    ]
    max-attempts: 3
    retry-delay: 0:00:02
    request: func [
        {Sends a GraphQL query and returns the decoded response}
        query [string! word!] "Query text or name of a predefined query"
        variables [map! none!]
        /local data tries result response
    ] [
        if none? header/Authorization: user's github-token [
            fail "Authorization token (github-token) is missing!"
        ]
        if word? :query [
            unless result: select queries :query [
                print [as-purple "*** Unknown query:" as-red :query]
                return none
            ]
            query: :result
        ]
        data: make map! 8
        data/query: query
        if variables [data/variables: variables]
        tries: max-attempts
        forever [
            response: try [
                *send https://api.github.com/graphql reduce ['POST header to-json data]
            ]
            case [
                error? response []
                response/1 >= 500 []
                response/1 >= 400 [
                    fail ajoin ["GitHub GraphQL error " response/1 ": " to string! response/3]
                ]
                true [
                    result: load-json to string! response/3
                    if all [block? result/errors none? result/data] [
                        fail ajoin ["GitHub GraphQL error: " result/errors/1/message]
                    ]
                    return result
                ]
            ]
            if 0 >= tries: tries - 1 [
                if error? response [do response]
                fail ajoin ["GitHub GraphQL error " response/1 ": " to string! response/3]
            ]
            wait retry-delay
        ]
    ]
    *send: func [
        {Sends an HTTP request and returns [status headers body]}
        url [url!]
        request [block!]
    ] [
        write/all url request
    ]
]
comment "-- End of:  %github-graphql.reb"
github-query: :graphql/request
github-repo: func [
    "Initialize the repository used for REST API calls."
    repo [block! path!]
] [
    rest/use-repo first repo second repo
    ()
]
github-run: :rest/run
github-get: :rest/get
github-post: :rest/post
github-edit: :rest/edit
